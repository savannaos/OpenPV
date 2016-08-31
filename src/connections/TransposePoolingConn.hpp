/*
 * TransposePoolingConn.cpp
 *
 *  Created on: March 25, 2015
 *      Author: slundquist
 */

#ifndef TRANSPOSEPOOLINGCONN_HPP_
#define TRANSPOSEPOOLINGCONN_HPP_

#include "HyPerConn.hpp"
#include "PoolingConn.hpp"
#include "layers/PoolingIndexLayer.hpp"
#ifdef PV_USE_CUDA
#include "cudakernels/CudaTransposePoolingDeliverKernel.hpp"
#endif // PV_USE_CUDA

namespace PV {

class TransposePoolingConn: public HyPerConn {
protected:
   virtual void ioParam_receiveGpu(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_sharedWeights(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_initializeFromCheckpointFlag(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_numAxonalArbors(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_plasticityFlag(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_triggerLayerName(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_pvpatchAccumulateType(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_writeStep(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_combine_dW_with_W_flag(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_nxp(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_nyp(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_nfp(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_dWMax(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_keepKernelsSynchronized(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_weightUpdatePeriod(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_initialWeightUpdateTime(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_shrinkPatches(enum ParamsIOFlag ioFlag) override;
   virtual void ioParam_originalConnName(enum ParamsIOFlag ioFlag);

public:
   TransposePoolingConn();
   TransposePoolingConn(const char * name, HyPerCol * hc);
   virtual ~TransposePoolingConn();
   virtual int allocateDataStructures();
   inline PoolingConn * getOriginalConn() {return mOriginalConn;}

   virtual bool needUpdate(double timed, double dt);
   virtual int updateState(double time, double dt);
   virtual double computeNewWeightUpdateTime(double time, double currentUpdateTime);
   virtual int checkpointRead(const char * cpDir, double const * timeptr) override;
   virtual int checkpointWrite(bool suppressCheckpointIfConstant, char const * cpDir, double timestamp);

protected:
    int initialize_base();
    int initialize(const char * name, HyPerCol * hc);
    virtual int ioParamsFillGroup(enum ParamsIOFlag ioFlag) override;
    virtual void setWeightInitializer() override;
    virtual void setWeightNormalizer() override;
    virtual int communicateInitInfo(std::shared_ptr<CommunicateInitInfoMessage const> message) override;
    virtual int setPatchSize();
#ifdef PV_USE_CUDA
    int initializeTransposePoolingDeliverKernelArgs();
#endif // PV_USE_CUDA
    virtual int setInitialValues();
    virtual int constructWeights();
    virtual int deliverPresynapticPerspective(PVLayerCube const * activity, int arborID) override;
    virtual int deliverPostsynapticPerspective(PVLayerCube const * activity, int arborID) override;
 #ifdef PV_USE_CUDA
    virtual int deliverPresynapticPerspectiveGPU(PVLayerCube const * activity, int arborID) override;
    virtual int deliverPostsynapticPerspectiveGPU(PVLayerCube const * activity, int arborID) override;
    int deliverGPU(PVLayerCube const * activity, int arborID);
 #endif

private:
    int deleteWeights();

// Member variables
protected:
    char * mOriginalConnName = nullptr;
    PoolingConn * mOriginalConn = nullptr;
    PoolingConn::AccumulateType mPoolingType = PoolingConn::MAX;
#ifdef PV_USE_CUDA
    PVCuda::CudaTransposePoolingDeliverKernel * mTransposePoolingDeliverKernel = nullptr;
#endif // PV_USE_CUDA
}; // end class TransposePoolingConn

}  // end namespace PV

#endif /* TRANSPOSECONN_HPP_ */
