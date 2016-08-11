/*
 * DataStore.hpp
 *
 *  Created on: Sep 10, 2008
 *      Author: Craig Rasmussen
 */

#ifndef DATASTORE_HPP_
#define DATASTORE_HPP_

#include "include/pv_arch.h"
#include "include/pv_types.h"
#include <cstdlib>
#include <cstring>
#include <vector>

namespace PV
{

class DataStore
{
public:
   DataStore(int numBuffers, int numItems, int numLevels, bool isSparse);

   virtual ~DataStore() {}

   int numberOfLevels()  {return numLevels;}
   int numberOfBuffers() {return numBuffers;}
   int newLevelIndex() {
      return (curLevel = (numLevels + curLevel - 1) % numLevels);
   }

   //Level (delay) spins slower than bufferId (batch element)

   pvdata_t * buffer(int bufferId, int level) {
      return mBuffer[levelIndex(level)].data()+bufferId*numItems;
   }

   pvdata_t * buffer(int bufferId) {
      return mBuffer[curLevel].data()+bufferId*numItems;
   }

   double getLastUpdateTime(int bufferId, int level) {
      return mLastUpdateTimes[levelIndex(level)][bufferId];
   }

   double getLastUpdateTime(int bufferId) {
      return mLastUpdateTimes[levelIndex(0)][bufferId];
   }

   void setLastUpdateTime(int bufferId, int level, double t) {
      mLastUpdateTimes[levelIndex(level)][bufferId] = t;
   }

   void setLastUpdateTime(int bufferId, double t) {
      mLastUpdateTimes[curLevel][bufferId] = t;
   }

   bool isSparse() {return isSparse_flag;}

   unsigned int* activeIndicesBuffer(int bufferId, int level) {
      return mActiveIndices[levelIndex(level)].data()+bufferId*numItems;
   }

   unsigned int* activeIndicesBuffer(int bufferId) {
      return mActiveIndices[curLevel].data()+bufferId*numItems;
   }

   void setNumActive(int bufferId, long numActive) {
      mNumActive[curLevel][bufferId] = numActive;
   }

   long * numActiveBuffer(int bufferId, int level){
      return mNumActive[levelIndex(level)].data()+bufferId;
   }

   long * numActiveBuffer(int bufferId){
      return mNumActive[curLevel].data()+bufferId;
   }

   int getNumItems(){ return numItems;}

private:
   int levelIndex(int level) { return ((level + curLevel) % numLevels); }

private:
   int   numItems;
   int   curLevel;
   int   numLevels;
   int   numBuffers;
   bool  isSparse_flag;

   std::vector<std::vector<pvdata_t> > mBuffer;
   std::vector<std::vector<long> > mNumActive;
   std::vector<std::vector<unsigned int> > mActiveIndices;
   std::vector<std::vector<double> > mLastUpdateTimes;
};

} // NAMESPACE

#endif /* DATASTORE_HPP_ */
