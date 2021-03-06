/*
 * InitSmartWeights.hpp
 *
 *  Created on: Aug 8, 2011
 *      Author: kpeterson
 */

#ifndef INITSMARTWEIGHTS_HPP_
#define INITSMARTWEIGHTS_HPP_

#include "InitWeights.hpp"
#include "InitWeightsParams.hpp"

namespace PV {

class InitSmartWeights : public PV::InitWeights {
  public:
   InitSmartWeights(char const *name, HyPerCol *hc);
   InitSmartWeights();
   virtual ~InitSmartWeights();

   virtual InitWeightsParams *createNewWeightParams();

   virtual int calcWeights(/* PVPatch * patch */ float *dataStart, int patchIndex, int arborId);

  protected:
   int initialize(char const *name, HyPerCol *hc);

  private:
   int initialize_base();
   int smartWeights(/* PVPatch * patch */ float *dataStart, int k, InitWeightsParams *weightParams);
}; // class InitSmartWeights

} /* namespace PV */
#endif /* INITSMARTWEIGHTS_HPP_ */
