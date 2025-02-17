
/** 
 * @file BumpHunter.cxx
 * @brief
 * @author Omar Moreno <omoreno1@ucsc.edu>
 *         Santa Cruz Institute for Particle Physics
 *         University of California, Santa Cruz
 * @date January 14, 2015
 *
 */

#include <BumpHunter.h>

BumpHunter::BumpHunter(BkgModel model, FitFunction::FitModel m_fit_model, int poly_order, int res_factor) 
    : ofs(nullptr),
      res_factor_(res_factor), 
      poly_order_(poly_order) {

}

BumpHunter::~BumpHunter() {
    delete printer; 
}

void BumpHunter::initialize(TH1* histogram, double &mass_hypothesis) { 

    mass_hypothesis_ = mass_hypothesis; 

    // Shift the mass hypothesis so it sits in the middle of a bin.  
    std::cout << "[ BumpHunter ]: Shifting mass hypothesis to nearest bin center " 
              << mass_hypothesis << " ---> "; 
    int mbin = histogram->GetXaxis()->FindBin(mass_hypothesis); 
    mass_hypothesis = histogram->GetXaxis()->GetBinCenter(mbin);
    std::cout << mass_hypothesis << " GeV" << std::endl;

    // If the mass hypothesis is below the lower bound, throw an exception.  A 
    // search cannot be performed using an invalid value for the mass hypothesis.
    if (mass_hypothesis < lower_bound) {
        throw std::runtime_error("Mass hypothesis less than the lower bound!"); 
    }

    // Correct the mass to take into account the mass scale systematic
    corr_mass_ = this->correctMass(mass_hypothesis);

    // Get the mass resolution at the corrected mass 
    mass_resolution_ = this->getMassResolution(corr_mass_);
    std::cout << "[ BumpHunter ]: Mass resolution: " << mass_resolution_ << " GeV" << std::endl;

    // Calculate the fit window size
    window_size_ = mass_resolution_*res_factor_;
    this->printDebug("Window size: " + std::to_string(window_size_));

    // Find the starting position of the window. This is set to the low edge of 
    // the bin closest to the calculated value. If the start position falls 
    // below the lower bound of the histogram, set it to the lower bound.
    window_start_ = mass_hypothesis - window_size_/2;
    int window_start_bin = histogram->GetXaxis()->FindBin(window_start_);  
    window_start_ = histogram->GetXaxis()->GetBinLowEdge(window_start_bin);
    if (window_start_ < lower_bound) { 
        std::cout << "[ BumpHunter ]: Starting edge of window (" << window_start_ 
                  << " MeV) is below lower bound." << std::endl;
        window_start_bin = histogram->GetXaxis()->FindBin(lower_bound);
        window_start_ = histogram->GetXaxis()->GetBinLowEdge(window_start_bin);
    }
    std::cout << "[ BumpHunter ]: Setting starting edge of fit window to " 
              << window_start_ << " MeV." << std::endl;
    
    // Find the end position of the window.  This is set to the lower edge of 
    // the bin closest to the calculated value. If the window edge falls above
    // the upper bound of the histogram, set it to the upper bound.
    // Furthermore, check that the bin serving as the upper boundary contains
    // events. If the upper bound is shifted, reset the lower window bound.
    window_end_ = window_start_ + window_size_;
    int window_end_bin = histogram->GetXaxis()->FindBin(window_end_);
    window_end_ = histogram->GetXaxis()->GetBinLowEdge(window_end_bin);
    if (window_end_ > upper_bound_) { 
        std::cout << "[ BumpHunter ]: Upper edge of window (" << window_end_ 
                  << " MeV) is above upper bound." << std::endl;
        window_end_bin = histogram->GetXaxis()->FindBin(upper_bound_);
        
        int last_bin_above = histogram->FindLastBinAbove(); 
        if (window_end_bin > last_bin_above) window_end_bin = last_bin_above; 
        
        window_end_ = histogram->GetXaxis()->GetBinLowEdge(window_end_bin);
        window_start_bin = histogram->GetXaxis()->FindBin(window_end_ - window_size_);  
        window_start_ = histogram->GetXaxis()->GetBinLowEdge(window_start_bin);
    }
    std::cout << "[ BumpHunter ]: Setting upper edge of fit window to " 
              << window_end_ << " MeV." << std::endl;

    // Estimate the background normalization within the window by integrating
    // the histogram within the window range.  This should be close to the 
    // background yield in the case where there is no signal present.
    integral_ = histogram->Integral(window_start_bin, window_end_bin);
    this->printDebug("Window integral: " + std::to_string(integral_)); 
}

HpsFitResult* BumpHunter::performSearch(TH1 *histogram, double mass_hypothesis, bool skip_bkg_fit = false) {
	// Calculate all of the fit parameters e.g. window size, mass hypothesis
	this->initialize(histogram, mass_hypothesis);
	
	// Instantiate a new fit result object to store all of the results.
	HpsFitResult *fit_result = new HpsFitResult();
	fit_result->setPolyOrder(poly_order_);
	
	// If not fitting toys, start by performing a background only fit.
	if(!skip_bkg_fit) {
		// Start by performing a background only fit.  The results from this fit 
		// are used to get an intial estimate of the background parameters.  
		std::cout << "*************************************************" << std::endl;
		std::cout << "*************************************************" << std::endl;
		std::cout << "[ BumpHunter ]: Performing a background only fit." << std::endl;
		std::cout << "*************************************************" << std::endl;
		std::cout << "*************************************************" << std::endl;
		
		//
		TF1 *bkg{nullptr};
		if(poly_order_ == 3) {
			//
			ExpPol3FullFunction bkg_func(FitFunction::FitModel::NONE, mass_hypothesis, window_end_ - window_start_); 
			bkg = new TF1("bkg", bkg_func, -1, 1, 4);
			
			//
			bkg->SetParameters(4, 0, 0, 0);
			bkg->SetParNames("pol0", "pol1", "pol2", "pol3");
		} else { 
			//
			ExpPol5FullFunction bkg_func(FitFunction::FitModel::NONE, mass_hypothesis, window_end_ - window_start_);
			bkg = new TF1("bkg", bkg_func, -1, 1, 6);
			
			//
			bkg->SetParameters(4, 0, 0, 0, 0, 0);
			bkg->SetParNames("pol0", "pol1", "pol2", "pol3", "pol4", "pol5");
		}
		
		std::cout << "Starting fit..." << std::endl;
		TFitResultPtr result = histogram->Fit("bkg", "LES+", "", window_start_, window_end_);
		std::cout << "End of fit..." << std::endl;
		fit_result->setBkgFitResult(result);
		//result->Print();
	}
	
	std::cout << "End background fit." << std::endl;
	
	std::cout << "***************************************************" << std::endl;
	std::cout << "***************************************************" << std::endl;
	std::cout << "[ BumpHunter ]: Performing a signal+background fit." << std::endl;
	std::cout << "***************************************************" << std::endl;
	std::cout << "***************************************************" << std::endl;
	
	TF1 *full{nullptr};
	if (poly_order_ == 3) {
		ExpPol3FullFunction full_func(fit_model, mass_hypothesis, window_end_ - window_start_);
		full = new TF1("full", full_func, -1, 1, 7);
		
		full->SetParameters(4, 0, 0, 0, 0, 0, 0);
		full->SetParNames("pol0", "pol1", "pol2", "pol3", "signal norm", "mean", "sigma");
		full->FixParameter(5, 0.0);
		full->FixParameter(6, mass_resolution_);
	} else {
		ExpPol5FullFunction full_func(fit_model, mass_hypothesis, window_end_ - window_start_);
		full = new TF1("full", full_func, -1, 1, 9);
		
		full->SetParameters(4, 0, 0, 0, 0, 0, 0, 0, 0);
		full->SetParNames("pol0", "pol1", "pol2", "pol3", "pol4", "pol5", "signal norm", "mean", "sigma");
		full->FixParameter(7, 0.0);
		full->FixParameter(8, mass_resolution_);
	}
	
	TFitResultPtr full_result = histogram->Fit("full", "LES+", "", window_start_, window_end_);
	fit_result->setCompFitResult(full_result);
	//full_result->Print();
	
	//if (!batch) { 
		//printer->print(histogram, window_start_, window_end_, "test_print.pdf");
	//}
	
	this->calculatePValue(fit_result);
	
	// This code doesn't work. Why? Needs to be fixed.
	// this->getUpperLimit(histogram, fit_result);
	
	// Persist the mass hypothesis used for this fit
	fit_result->setMass(mass_hypothesis_);
	
	// Persist the mass after correcting for the mass scale
	fit_result->setCorrectedMass(corr_mass_);
	
	// Set the window size 
	fit_result->setWindowSize(window_size_);
	
	// Set the total number of events within the window
	fit_result->setIntegral(integral_);
	
	return fit_result; 
}

void BumpHunter::calculatePValue(HpsFitResult* result) {

    std::cout << "[ BumpHunter ]: Calculating p-value: " << std::endl;

    //
    double signal_yield = result->getCompFitResult()->Parameter(poly_order_ + 1); 
    this->printDebug("Signal yield: " + std::to_string(signal_yield)); 

    // In searching for a resonance, a signal is expected to lead to an 
    // excess of events.  In this case, a signal yield of < 0 is  
    // meaningless so we set the p-value = 1.  This follows the formulation
    // put forth by Cowen et al. in https://arxiv.org/pdf/1007.1727.pdf. 
    if (signal_yield <= 0) { 
        result->setPValue(1);
        this->printDebug("Signal yield is negative ... setting p-value = 1"); 
        return; 
    }

    // Get the NLL obtained by minimizing the composite model with the signal
    // yield floating.
    double mle_nll = result->getCompFitResult()->MinFcnValue(); 
    this->printDebug("NLL when mu = " + std::to_string(signal_yield) + ": " + std::to_string(mle_nll));

    // Get the NLL obtained from the Bkg only fit.
    double cond_nll = result->getBkgFitResult()->MinFcnValue(); 
    printDebug("NLL when mu = 0: " + std::to_string(cond_nll));

    // 1) Calculate the likelihood ratio which is chi2 distributed. 
    // 2) From the chi2, calculate the p-value.
    double q0 = 0; 
    double p_value = 0; 
    this->getChi2Prob(cond_nll, mle_nll, q0, p_value);  

    std::cout << "[ BumpHunter ]: p-value: " << p_value << std::endl;
    
    // Update the result
    result->setPValue(p_value);
    result->setQ0(q0);  
    
}

void BumpHunter::printDebug(std::string message) { 
    if (debug) std::cout << "[ BumpHunter ]: " << message << std::endl;
}

void BumpHunter::getUpperLimit(TH1* histogram, HpsFitResult* result) {
    
    TF1* comp{nullptr};
    if (poly_order_ == 3) { 
  
        ExpPol3FullFunction comp_func(fit_model, mass_hypothesis_, window_end_ - window_start_); 
        comp = new TF1("comp_ul", comp_func, -1, 1, 7);
    
        comp->SetParameters(4,0,0,0,0,0,0);
        comp->SetParNames("pol0","pol1","pol2","pol3","signal norm","mean","sigma");
        comp->FixParameter(5,0.0); 
        comp->FixParameter(6, mass_resolution_); 
        
    } else { 
       
        ExpPol5FullFunction comp_func(fit_model, mass_hypothesis_, window_end_ - window_start_); 
        comp = new TF1("comp_ul", comp_func, -1, 1, 9);
    
        comp->SetParameters(4,0,0,0,0,0,0,0,0);
        comp->SetParNames("pol0","pol1","pol2","pol3","pol4","pol5","signal norm","mean","sigma");
        comp->FixParameter(7,0.0); 
        comp->FixParameter(8, mass_resolution_);
   
    }    

    std::cout << "Mass resolution: " << mass_resolution_ << std::endl; 
    std::cout << "[ BumpHunter ]: Calculating upper limit." << std::endl;

    //  Get the signal yield obtained from the signal+bkg fit
    double signal_yield = result->getCompFitResult()->Parameter(poly_order_ + 1); 
    this->printDebug("Signal yield: " + std::to_string(signal_yield)); 

    // Get the minimum NLL value that will be used for testing upper limits.
    // If the signal yield (mu estimator) at the min NLL is < 0, use the NLL
    // obtained when mu = 0.
    double mle_nll = result->getCompFitResult()->MinFcnValue(); 

    if (signal_yield < 0) {
        this->printDebug("Signal yield @ min NLL is < 0. Using NLL when signal yield = 0");

        // Get the NLL obtained assuming the background only hypothesis
        mle_nll = result->getBkgFitResult()->MinFcnValue(); 
        
        signal_yield = 0;
    } 
    this->printDebug("MLE NLL: " + std::to_string(mle_nll));   

    signal_yield = floor(signal_yield) + 1; 

    double p_value = 1;
    double q0 = 0; 
    while(true) {
       
        this->printDebug("Setting signal yield to: " + std::to_string(signal_yield));
        std::cout << "[ BumpHunter ]: Current p-value: " << p_value << std::endl;
        comp->FixParameter(poly_order_ + 1, signal_yield); 
        
        TFitResultPtr full_result 
            = histogram->Fit("comp_ul", "LES+", "", window_start_, window_end_);
        double cond_nll = full_result->MinFcnValue(); 

        // 1) Calculate the likelihood ratio which is chi2 distributed. 
        // 2) From the chi2, calculate the p-value.
        this->getChi2Prob(cond_nll, mle_nll, q0, p_value);  

        this->printDebug("p-value after fit : " + std::to_string(p_value)); 
        
        if ((p_value <= 0.0455 && p_value > 0.044)) { 
            
            std::cout << "[ BumpHunter ]: Upper limit: " << signal_yield << std::endl;
            std::cout << "[ BumpHunter ]: p-value: " << p_value << std::endl;

            result->setUpperLimit(signal_yield);
            result->setUpperLimitPValue(p_value); 
     
            break; 
        } else if (p_value <= 0.044) {
            signal_yield -= 1;
        } else if (p_value <= 0.059) signal_yield += 1;
        else if (p_value <= 0.10) signal_yield += 20;
        else if (p_value <= 0.2) signal_yield += 40; 
        else signal_yield += 100;  
    }
}

std::vector<TH1*> BumpHunter::generateToys(TH1* histogram, double n_toys, int seed) { 

    gRandom->SetSeed(seed); 

    TF1* bkg = histogram->GetFunction("bkg"); 

    std::vector<TH1*> hists;
    for (int itoy = 0; itoy < n_toys; ++itoy) { 
        std::string name = "invariant_mas_" + std::to_string(itoy); 
        std::cout << "Total number of bins: " << histogram->GetNbinsX() << std::endl;
        TH1F* hist = new TH1F(name.c_str(), name.c_str(), histogram->GetNbinsX(), 0., 0.15);
        for (int i =0; i < integral_; ++i) { 
            hist->Fill(bkg->GetRandom(window_start_, window_end_)); 
        }
        hists.push_back(hist); 
    }

    return hists;
}


void BumpHunter::getChi2Prob(double cond_nll, double mle_nll, double &q0, double &p_value) {
   

    this->printDebug("Cond NLL: " + std::to_string(cond_nll)); 
    this->printDebug("Uncod NLL: " + std::to_string(mle_nll));  
    double diff = cond_nll - mle_nll;
    this->printDebug("Delta NLL: " + std::to_string(diff));
    
    q0 = 2*diff;
    this->printDebug("q0: " + std::to_string(q0));
    
    p_value = ROOT::Math::chisquared_cdf_c(q0, 1);
    this->printDebug("p-value before dividing: " + std::to_string(p_value));  
    p_value *= 0.5;
    this->printDebug("p-value: " + std::to_string(p_value)); 
}

void BumpHunter::setBounds(double lower_bound, double upper_bound) {
    lower_bound = lower_bound; 
    upper_bound_ = upper_bound;
    printf("Fit bounds set to [ %f , %f ]\n", lower_bound, upper_bound_);   
}

double BumpHunter::correctMass(double mass) { 
    double offset = -1.19892320e4*pow(mass, 3) + 1.50196798e3*pow(mass,2) 
                    - 8.38873712e1*mass + 6.23215746; 
    offset /= 100; 
    this->printDebug("Offset: " + std::to_string(offset)); 
    double cmass = mass - mass*offset; 
    this->printDebug("Corrected Mass: " + std::to_string(cmass)); 
    return cmass;
}
