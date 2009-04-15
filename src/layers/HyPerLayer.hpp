/*
 * HyPerLayer.hpp
 *
 *  Created on: Aug 3, 2008
 *      Author: dcoates
 */

#ifndef HYPERLAYER_HPP_
#define HYPERLAYER_HPP_

#include "../layers/PVLayer.h"
#include "../columns/DataStore.hpp"
#include "../columns/HyPerCol.hpp"
#include "../columns/InterColComm.hpp"
#include "../io/PVLayerProbe.hpp"
#include "../include/pv_types.h"

namespace PV {

class HyPerLayer {

   friend class HyPerCol;

protected:

   HyPerLayer();
   HyPerLayer(const char* name, HyPerCol * hc);
   virtual ~HyPerLayer() = 0;

public:

   // TODO - make protected
   PVLayer*  clayer;
   HyPerCol* parent;

   virtual int updateState(float time, float dt) = 0;

   virtual int
       recvSynapticInput(HyPerConn * conn, PVLayerCube * cube, int neighbor);

   virtual int init(const char * name, PVLayerType type);
   virtual int initBorder(PVLayerCube * border, int borderId);
   virtual int initFinish();

   virtual int columnWillAddLayer(InterColComm * comm, int id);
   virtual int copyToBorder(int whichBorder, PVLayerCube * cube, PVLayerCube * borderCube);

   virtual int setParams(int numParams, size_t sizeParams, float * params);
   virtual int getParams(int * numParams, float ** params);
   virtual int setFuncs(void * initFunc, void * updateFunc);

   virtual int publish(InterColComm* comm, float time);
   virtual int outputState(float time);
   virtual int writeState(const char * path, float time);

   virtual int insertProbe(PVLayerProbe * probe);

   virtual int  getActivityLength();

   virtual int copyToNorthWest(PVLayerCube * dest, PVLayerCube * src);
   virtual int copyToNorth    (PVLayerCube * dest, PVLayerCube* src);
   virtual int copyToNorthEast(PVLayerCube * dest, PVLayerCube * src);
   virtual int copyToWest     (PVLayerCube * dest, PVLayerCube * src);
   virtual int copyToEast     (PVLayerCube * dest, PVLayerCube * src);
   virtual int copyToSouthWest(PVLayerCube * dest, PVLayerCube * src);
   virtual int copyToSouth    (PVLayerCube * dest, PVLayerCube * src);
   virtual int copyToSouthEast(PVLayerCube * dest, PVLayerCube * src);

   // Public access functions:

   const char * getName()            {return clayer->name;}

   int  getLayerId()                 {return clayer->layerId;}
   void setLayerId(int id)           {clayer->layerId = id;}

   void setOutputOnPublish(int flag) {outputOnPublish = flag;}

   PVLayer*  getCLayer()             {return clayer;}

   HyPerCol* getParent()             {return parent;}
   void setParent(HyPerCol* parent)  {this->parent = parent;}

protected:
   virtual int initGlobal(int colId, int colRow, int colCol, int nRows, int nCols);

   PVLayerProbe * probe;
   int maxBorderSize;
   int outputOnPublish;
   int ioAppend;                // controls opening of binary files
};

} // namespace PV

#endif /* HYPERLAYER_HPP_ */
