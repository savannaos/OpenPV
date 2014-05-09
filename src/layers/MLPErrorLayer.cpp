
/*
 * MLPErrorLayer.cpp
 *
 *  Created on: Mar 21, 2014
 *      Author: slundquist 
 */

#include "MLPErrorLayer.hpp"
#include "../include/default_params.h"

//#ifdef __cplusplus
//extern "C" {
//#endif
//
//void MLPErrorLayer_update_state(
//    const int numNeurons,
//    const int nx,
//    const int ny,
//    const int nf,
//    const int nb,
//
//    float * V,
//    const float Vth,
//    const float VMax,
//    const float VMin,
//    const float VShift,
//    float * GSynHead,
//    float * activity
//    );
//
//
//#ifdef __cplusplus
//}
//#endif

namespace PV {

MLPErrorLayer::MLPErrorLayer()
{
   initialize_base();
}

MLPErrorLayer::MLPErrorLayer(const char * name, HyPerCol * hc)
{
   initialize_base();
   initialize(name, hc);
}

MLPErrorLayer::~MLPErrorLayer()
{
   if(forwardLayername) free(forwardLayername);
   forwardLayer = NULL;
   clayer->V = NULL;
}

int MLPErrorLayer::initialize_base()
{
   dropout = NULL;
   forwardLayername = NULL;
   linAlpha = 0;
   symSigmoid = true;
   lossFunction = (char *)"squared"; //This should be hidden, but for backwards compatibility, default is squared
   lastError = false;
   return PV_SUCCESS;
}

int MLPErrorLayer::initialize(const char * name, HyPerCol * hc)
{
   int status = ANNLayer::initialize(name, hc);
   return status;
}

int MLPErrorLayer::communicateInitInfo(){
   int status = ANNLayer::communicateInitInfo();
   HyPerLayer* tmpLayer = parent->getLayerFromName(forwardLayername);
   if (tmpLayer==NULL) {
      if (parent->columnId()==0) {
         fprintf(stderr, "%s \"%s\" error: ForwardLayername \"%s\" is not a layer in the HyPerCol.\n",
                 parent->parameters()->groupKeywordFromName(name), name, forwardLayername);
      }
#ifdef PV_USE_MPI
      MPI_Barrier(parent->icCommunicator()->communicator());
#endif
      exit(EXIT_FAILURE);
   }
   forwardLayer = dynamic_cast<MLPForwardLayer*>(tmpLayer);
   if (forwardLayer==NULL) {
      if (parent->columnId()==0) {
         fprintf(stderr, "%s \"%s\" error: ForwardLayername \"%s\" is not a MLPErrorLayer.\n",
                 parent->parameters()->groupKeywordFromName(name), name, forwardLayername);
      }
#ifdef PV_USE_MPI
      MPI_Barrier(parent->icCommunicator()->communicator());
#endif
      exit(EXIT_FAILURE);
   }
   const PVLayerLoc * srcLoc = forwardLayer->getLayerLoc();
   const PVLayerLoc * loc = getLayerLoc();
   assert(srcLoc != NULL && loc != NULL);
   if (srcLoc->nxGlobal != loc->nxGlobal || srcLoc->nyGlobal != loc->nyGlobal || srcLoc->nf != loc->nf) {
      if (parent->columnId()==0) {
         fprintf(stderr, "%s \"%s\" error: ForwardLayerName \"%s\" does not have the same dimensions.\n",
                 parent->parameters()->groupKeywordFromName(name), name, forwardLayername);
         fprintf(stderr, "    original (nx=%d, ny=%d, nf=%d) versus (nx=%d, ny=%d, nf=%d)\n",
                 srcLoc->nxGlobal, srcLoc->nyGlobal, srcLoc->nf, loc->nxGlobal, loc->nyGlobal, loc->nf);
      }
#ifdef PV_USE_MPI
      MPI_Barrier(parent->icCommunicator()->communicator());
#endif
      exit(EXIT_FAILURE);
   }
   assert(srcLoc->nx==loc->nx && srcLoc->ny==loc->ny);
   dropout = forwardLayer->getDropout();
   return status;
}

int MLPErrorLayer::allocateDataStructures() {
   assert(forwardLayer);
   int status = PV_SUCCESS;
   // Make sure forwardLayer has allocated its V buffer before copying its address to clone's V buffer
   if (forwardLayer->getDataStructuresAllocatedFlag()) {
      status = HyPerLayer::allocateDataStructures();
   }
   else {
      status = PV_POSTPONE;
   }
   return status;
}

int MLPErrorLayer::allocateV() {
   assert(forwardLayer && forwardLayer->getCLayer());
   //Set own V to forward layer's V
   clayer->V = forwardLayer->getV();
   if (getV()==NULL) {
      fprintf(stderr, "%s \"%s\": forwardLayer \"%s\" has a null V buffer in rank %d process.\n",
              parent->parameters()->groupKeywordFromName(name), name, forwardLayername, parent->columnId());
      exit(EXIT_FAILURE);
   }
   return PV_SUCCESS;
}

int MLPErrorLayer::initializeV() {
   // TODO Need to make sure that original layer's initializeState has been called before this layer's V.
   return PV_SUCCESS;
}

int MLPErrorLayer::checkpointRead(const char * cpDir, double * timed) {
   // If we just call HyPerLayer, we checkpoint V since it is non-null.  This is redundant since V is a clone.
   // So we temporarily set clayer->V to NULL to fool HyPerLayer::checkpointRead into not reading it.
   // A cleaner way to do this would be to have HyPerLayer::checkpointRead call a virtual method checkpointReadV, which can be overridden if unnecessary.
   pvdata_t * V = clayer->V;
   clayer->V = NULL;
   int status = HyPerLayer::checkpointRead(cpDir, timed);
   clayer->V = V;
   return status;
}

int MLPErrorLayer::checkpointWrite(const char * cpDir) {
   pvdata_t * V = clayer->V;
   clayer->V = NULL;
   int status = HyPerLayer::checkpointWrite(cpDir);
   clayer->V = V;
   return status;
}

int MLPErrorLayer::ioParamsFillGroup(enum ParamsIOFlag ioFlag) {
   int status = ANNLayer::ioParamsFillGroup(ioFlag);
   ioParam_ForwardLayername(ioFlag);
   ioParam_LossFunction(ioFlag);
   ioParam_lastError(ioFlag);
   ioParam_symSigmoid(ioFlag);
   if(!symSigmoid){
      ioParam_Vrest(ioFlag);
      ioParam_VthRest(ioFlag);
      ioParam_SigmoidAlpha(ioFlag);
   }
   else{
      ioParam_LinAlpha(ioFlag);
   }
   return status;
}

void MLPErrorLayer::ioParam_symSigmoid(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "symSigmoid", &symSigmoid, symSigmoid, true/*warnIfAbsent*/);
}

void MLPErrorLayer::ioParam_LinAlpha(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "linAlpha", &linAlpha, linAlpha);
}

void MLPErrorLayer::ioParam_Vrest(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "Vrest", &Vrest, (float) V_REST);
}
void MLPErrorLayer::ioParam_VthRest(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "VthRest", &VthRest, (float) VTH_REST);
}
void MLPErrorLayer::ioParam_SigmoidAlpha(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "SigmoidAlpha", &sigmoid_alpha, (float) SIGMOIDALPHA);
}

void MLPErrorLayer::ioParam_ForwardLayername(enum ParamsIOFlag ioFlag) {
   parent->ioParamStringRequired(ioFlag, name, "ForwardLayername", &forwardLayername);
}

void MLPErrorLayer::ioParam_lastError(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "lastError", &lastError, lastError, true/*warnIfAbsent*/);
}

void MLPErrorLayer::ioParam_LossFunction(enum ParamsIOFlag ioFlag) {
   parent->ioParamString(ioFlag, name, "lossFunction", &lossFunction, lossFunction);
   if(strcmp(lossFunction, "squared") == 0){
   }
   else if(strcmp(lossFunction, "entropy") == 0){
   }
   else if(strcmp(lossFunction, "hidden") == 0){
   }
   else{
      fprintf(stderr, "%s \"%s\" error: Loss function not defined. Options are \"squared\", \"entropy\", or \"hidden\".\n",
           parent->parameters()->groupKeywordFromName(name), name);
   }
}

int MLPErrorLayer::updateState(double time, double dt)
{
   update_timer->start();
   //assert(getNumChannels()>= 3);

   const PVLayerLoc * loc = getLayerLoc();

   int nx = loc->nx;
   int ny = loc->ny;
   int nf = loc->nf;
   int num_neurons = nx*ny*nf;
   //Reset pointer of gSynHead to point to the inhib channel
   pvdata_t * GSynExt = getChannel(CHANNEL_EXC);
   pvdata_t * GSynInh = getChannel(CHANNEL_INH);

   pvdata_t Vth, sig_scale;
   if(!symSigmoid){
      //Calculate constants for derivitive of sigmoid layer
      Vth = (VthRest+Vrest)/2.0;
      sig_scale = -logf(1.0f/sigmoid_alpha - 1.0f)/(Vth - Vrest);
   }
   pvdata_t * A = getCLayer()->activity->data;
   pvdata_t * V = getV();

   for(int ni = 0; ni < num_neurons; ni++){
      int next = kIndexExtended(ni, nx, ny, nf, loc->nb);
      //Update activity
      //f'(V)*(error)
      //error = gt - finalLayer iff error is last error
      float errProp, gradient;
      //exct is expected, inh is actual
      if(lastError){
         //0 is DCR
         if(GSynExt[ni] == 0){
            errProp = 0;
         }
      }
      else{
         if(strcmp(lossFunction, "squared") == 0){
            //expected - actual
            errProp = GSynExt[ni] - GSynInh[ni];
         }
         else if(strcmp(lossFunction, "entropy") == 0){
            //expected/actual
            errProp = GSynExt[ni]/GSynInh[ni];
         }
         else if(strcmp(lossFunction, "hidden") == 0){
            errProp = GSynExt[ni];
         }
         if(symSigmoid){
            gradient = 1.14393 * (1/(pow(cosh(((float)2/3) * V[ni]), 2))) + linAlpha;
         }
         else{
            gradient = -.5 * sig_scale * (1/(pow(cosh(sig_scale*(Vth - V[ni])), 2)));
         }
      }
      A[next] = dropout[ni] ? 0 : errProp * gradient;
   }
   update_timer->stop();
   return PV_SUCCESS;
}

} /* namespace PV */


//#ifdef __cplusplus
//extern "C" {
//#endif
//
//#ifndef PV_USE_OPENCL
//#  include "../kernels/MLPErrorLayer_update_state.cl"
//#else
//#  undef PV_USE_OPENCL
//#  include "../kernels/MLPErrorLayer_update_state.cl"
//#  define PV_USE_OPENCL
//#endif
//
//#ifdef __cplusplus
//}
//#endif
