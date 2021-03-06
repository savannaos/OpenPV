/*
 * IndexWeightConn.hpp
 *
 *  Created on: Mar 2, 2017
 *      Author: pschultz
 *
 * A connection class with a built-in initialization method and a simple
 * update rule, to be used in testing.
 * The weights in each nxp-by-nyp-by-nfp are ordered 0,1,2,...,(nxp*nyp*nfp-1)
 * in standard PetaVision ordering. At initialization, the strength of the
 * weight at index k is k + start-time; when updateState is called, the
 * weight at index k becomes k + simulation-time.
 */

#ifndef INDEXWEIGHTCONN_HPP_
#define INDEXWEIGHTCONN_HPP_

#include <connections/HyPerConn.hpp>

namespace PV {

class IndexWeightConn : public HyPerConn {
  protected:
   virtual void ioParam_weightInitType(enum ParamsIOFlag ioFlag) override {}

  public:
   IndexWeightConn(const char *name, HyPerCol *hc);
   virtual ~IndexWeightConn();
   virtual int updateWeights(int axonId = 0);

  protected:
   int initialize(const char *name, HyPerCol *hc);
   virtual int setInitialValues() override;

}; // end class IndexWeightConn

} // end namespace PV block

#endif /* INDEXWEIGHTCONN_HPP_ */
