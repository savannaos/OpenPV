/*
 * DataStore.hpp
 *
 *  Created on: Sep 10, 2008
 *      Author: Craig Rasmussen
 */

#ifndef DATASTORE_HPP_
#define DATASTORE_HPP_

#include "../include/pv_arch.h"
#include <stdlib.h>

#ifdef PV_USE_OPENCL
#include "../arch/opencl/CLBuffer.hpp"
#include "../arch/opencl/CLDevice.hpp"
#endif

namespace PV
{
class HyPerCol;

class DataStore
{
public:
//#ifdef PV_USE_OPENCL
//   DataStore(HyPerCol * hc, int numBuffers, size_t size, int numLevels, bool copydstoreflag);
//#else
   DataStore(HyPerCol * hc, int numBuffers, int numItems, size_t dataSize, int numLevels, bool isSparse);
//#endif

   virtual ~DataStore();

   size_t size()         {return bufSize;}

   int numberOfLevels()  {return numLevels;}
   int numberOfBuffers() {return numBuffers;}
   int lastLevelIndex()
         {return ((numLevels + curLevel + 1) % numLevels);}
   int levelIndex(int level)
         {return ((level + curLevel) % numLevels);}
   int newLevelIndex()
         {return (curLevel = (numLevels + curLevel - 1) % numLevels);}
   void* buffer(int bufferId, int level)
         {return (recvBuffers + bufferId*numLevels*bufSize + levelIndex(level)*bufSize);}
   void* buffer(int bufferId)
         {return (recvBuffers + bufferId*numLevels*bufSize + curLevel*bufSize);}
   double getLastUpdateTime(int bufferId, int level) { return lastUpdateTimes[bufferId*numLevels+levelIndex(level)]; }
   double getLastUpdateTime(int bufferId) { return lastUpdateTimes[bufferId*numLevels+levelIndex(0)]; }
   void setLastUpdateTime(int bufferId, int level, double t) { lastUpdateTimes[bufferId*numLevels+levelIndex(level)] = t; }
   void setLastUpdateTime(int bufferId, double t) { lastUpdateTimes[bufferId*numLevels+levelIndex(0)] = t; }

   size_t bufferOffset(int bufferId, int level=0)
         {return (bufferId*numLevels*bufSize + levelIndex(level)*bufSize);}
   bool isSparse() {return isSparse_flag;}

   unsigned int* activeIndicesBuffer(int bufferId, int level)
         {return (activeIndices + bufferId*numLevels*numItems + levelIndex(level)*numItems);}

   unsigned int* activeIndicesBuffer(int bufferId){
      return (activeIndices + bufferId*numLevels*numItems + curLevel*numItems);
   }

   long * numActiveBuffer(int bufferId, int level){
      return (numActive + bufferId*numLevels + levelIndex(level));
   }

   long * numActiveBuffer(int bufferId){
      return (numActive + bufferId*numLevels + curLevel);
   }

   int getNumItems(){ return numItems;}

private:
   size_t dataSize;
   int    numItems;
   size_t bufSize;
   int    curLevel;
   int    numLevels;
   int    numBuffers;
   char*  recvBuffers;

   unsigned int*   activeIndices;
   long *   numActive;
   bool  isSparse_flag;
   double * lastUpdateTimes; // A ring buffer for the getLastUpdateTime() function.

//#ifdef PV_USE_OPENCL
//   CLBuffer * clRecvBuffers;
//   cl_event   evCopyDataStore;
//   int numWait;
//public:
//   int initializeThreadBuffers(HyPerCol * hc);
//   int copyBufferToDevice();
//   int waitForCopy();
//   CLBuffer * getCLBuffer() {return clRecvBuffers;}
//#endif
};

} // NAMESPACE

#endif /* DATASTORE_HPP_ */
