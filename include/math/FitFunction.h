#ifndef FITFUNCTION_H
#define FITFUNCTION_H

class FitFunction {
	public:
        enum FitModel {
            NONE         = 0,
            GAUSSIAN     = 1,
            CRYSTAL_BALL = 2
        };
		
		/** Constructor */
		FitFunction(FitModel model, double m_mass_hypothesis, double m_window_size) {
			window_size = m_window_size;
			mass_hypothesis = m_mass_hypothesis;
			fit_model = model;
		}
		
		/** Calculates the value of the function at the specified x
		 * and with the specified parameters.
		 */
		double operator() (double *x, double *par) {
			return calculateBackground(x, par) + calculateSignal(x, par);
		}
		
	protected:
		/** Mass hypothesis */
		double mass_hypothesis = 0;
		
		/** Size of the search window. */
		double window_size = 0;
		
		/** Fit function to be used. */
		FitModel fit_model;
		
		/**
		 * Calculates the value of the background function at the
		 * specified x and with the specified parameters.
		 */
		virtual double calculateBackground(double *x, double *par) = 0;
		
		/**
		 * Calculates the value of the signal function at the
		 * specified x and with the specified parameters.
		 */
		virtual double calculateSignal(double *x, double *par) = 0;
		
		/**
		 * Gets a value of x corrected for window size and the mass
		 * hypothesis.
		 */
		double getCorrectedX(double x) {
			return (x - mass_hypothesis) / (window_size * 2.0);
		}
};

#endif