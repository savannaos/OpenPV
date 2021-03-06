#ifndef _KNEETIMESCALECONTROLLER_HPP_
#define _KNEETIMESCALECONTROLLER_HPP_

#include "AdaptiveTimeScaleController.hpp"

namespace PV {

class KneeTimeScaleController : public AdaptiveTimeScaleController {
  public:
   KneeTimeScaleController(
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
         double kneeThresh,
         double kneeSlope);

   virtual std::vector<double>
   calcTimesteps(double timeValue, std::vector<double> const &rawTimeScales);

  protected:
   double mKneeThresh = 1.0;
   double mKneeSlope  = 1.0;
};
}

#endif
