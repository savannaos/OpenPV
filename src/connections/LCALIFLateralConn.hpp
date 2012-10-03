/*
 * LCALIFLateralConn.hpp
 *
 *  Created on: Oct 3, 2012
 *      Author: pschultz
 */

#ifndef LCALIFLATERALCONN_HPP_
#define LCALIFLATERALCONN_HPP_

#include "HyPerConn.hpp"
#include "../layers/LCALIFLayer.hpp"

namespace PV {

class LCALIFLateralConn: public PV::HyPerConn {

// Methods
public:
   LCALIFLateralConn(const char * name, HyPerCol * hc, HyPerLayer * pre, HyPerLayer * post, const char * filename, InitWeights * weightInit);
   virtual ~LCALIFLateralConn();
   virtual int updateWeights(int axonId = 0);

   float getIntegratedSpikeCount(int kex) {return integratedSpikeCount[kex];}
   float getIntegrationTimeConstant() {return integrationTimeConstant;}
   float getAdaptationTimeConstant() {return inhibitionTimeConstant;}

   virtual int setParams(PVParams * params); // Really should be protected

   virtual int checkpointWrite(const char * cpDir);
   virtual int checkpointRead(const char * cpDir, float* timef);

protected:
   LCALIFLateralConn();
   int initialize(const char * name, HyPerCol * hc, HyPerLayer * pre, HyPerLayer * post, const char * filename, InitWeights * weightInit);
   virtual int calc_dW(int axonId = 0);

   virtual float readIntegrationTimeConstant() {return getParent()->parameters()->value(name, "integrationTimeConstant", 1.0);}
   virtual float readAdaptationTimeConstant() {return getParent()->parameters()->value(name, "adaptationTimeConstant", 0.0);}

private:
   int initialize_base();

// Member variables
protected:
   float * integratedSpikeCount; // The leaky count of spikes (the weight is a decaying exponential of time since that spike)
   float integrationTimeConstant; // often the same as the the LCALIFLayer's tau_LCA
   float inhibitionTimeConstant;
};

} /* namespace PV */
#endif /* LCALIFLATERALCONN_HPP_ */
