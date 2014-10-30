/*
 * PoolingANNLayer.cpp
 *
 * The output V is determined from GSynExc and GSynInh
 * using the formula GSynExc*GSynInh*(biasExc*GSynExc+biasInh*GSynInh)
 * biasExc and biasInh are set by the params file parameter bias:
 * biasExc = (1+bias)/2;  biasInh = (1-bias)/2
 *
 * This type of expression arises in the pooling generative models
 * "Exc" and "Inh" are really misnomers for this class, but the
 * terminology is inherited from the base class.
 *
 *  Created on: Apr 20, 2011
 *      Author: peteschultz
 */

#include "PoolingANNLayer.hpp"

namespace PV {

PoolingANNLayer::PoolingANNLayer() {
   initialize_base();
}

PoolingANNLayer::PoolingANNLayer(const char * name, HyPerCol * hc) {
   initialize_base();
   initialize(name, hc);
}

int PoolingANNLayer::initialize_base() {
   return PV_SUCCESS;
}

int PoolingANNLayer::initialize(const char * name, HyPerCol * hc) {
   return ANNLayer::initialize(name, hc);
}  // end of PoolingANNLayer::initialize()

int PoolingANNLayer::ioParamsFillGroup(enum ParamsIOFlag ioFlag) {
   int status = ANNLayer::ioParamsFillGroup(ioFlag);
   ioParam_bias(ioFlag);
   return status;
}

void PoolingANNLayer::ioParam_bias(enum ParamsIOFlag ioFlag) {
   parent->ioParamValue(ioFlag, name, "bias", &bias, (pvdata_t) 0);
   if (ioFlag == PARAMS_IO_READ) {
      setBias(bias);
   }
}

int PoolingANNLayer::updateState(double timef, double dt) {
   int status;
   status = doUpdateState(timef, dt, getLayerLoc(), getCLayer()->activity->data, getV(), getNumChannels(), GSyn[0], getBiasa(), getBiasb());
   return status;
}

int PoolingANNLayer::doUpdateState(double timef, double dt, const PVLayerLoc * loc, pvdata_t * A, pvdata_t * V, int num_channels, pvdata_t * gSynHead, pvdata_t biasa, pvdata_t biasb) {
   int nx=loc->nx;
   int ny=loc->ny;
   int nf=loc->nf;
   int num_neurons = nx*ny*nf;
//   pvdata_t * gSynExc = getChannelStart(gSynHead, CHANNEL_EXC, num_neurons);
//   pvdata_t * gSynInh = getChannelStart(gSynHead, CHANNEL_INH, num_neurons);
   updateV_PoolingANNLayer(num_neurons, V, gSynHead, biasa, biasb);
   setActivity_HyPerLayer(num_neurons, A, V, nx, ny, nf, loc->halo.lt, loc->halo.rt, loc->halo.dn, loc->halo.up); // setActivity();
   resetGSynBuffers_HyPerLayer(num_neurons, getNumChannels(), gSynHead); // resetGSynBuffers();
   return PV_SUCCESS;
}

//int PoolingANNLayer::updateV() {
//   pvdata_t * V = getV();
//   pvdata_t * GSynExc = this->getChannel(CHANNEL_EXC);
//   pvdata_t * GSynInh = this->getChannel(CHANNEL_INH);
//   for( int k=0; k<getNumNeurons(); k++ ) {
//      V[k] = GSynExc[k]*GSynInh[k]*(getBiasa()*GSynExc[k]+getBiasb()*GSynInh[k]);
//   }
//   return PV_SUCCESS;
//}  // end of PoolingANNLayer::updateV()

}  // end of namespace PV block
