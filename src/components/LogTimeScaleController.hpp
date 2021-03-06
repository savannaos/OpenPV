#ifndef _LOGTIMESCALECONTROLLER_HPP_
#define _LOGTIMESCALECONTROLLER_HPP_

#include "AdaptiveTimeScaleController.hpp"
#include <cfloat>

namespace PV {

class LogTimeScaleController : public AdaptiveTimeScaleController {
  public:
   LogTimeScaleController(
         char const *name,
         int batchWidth,
         double baseMax,
         double baseMin,
         double tauFactor,
         double growthFactor,
         bool writeTimeScales,
         bool writeTimeScaleFieldnames,
         Communicator *comm,
         bool verifyWrites,
         double logThresh,
         double logSlope);

   virtual std::vector<double>
   calcTimesteps(double timeValue, std::vector<double> const &rawTimeScales);

  protected:
   double mLogThresh = DBL_MAX_EXP;
   double mLogSlope  = 1.0;
};
}

#endif
