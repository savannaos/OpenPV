/*
 * BaseObject.hpp
 *
 *  This is the base class for HyPerCol, layers, connections, probes, and
 *  anything else that the Factory object needs to know about.
 *
 *  All objects in the BaseObject hierarchy should have an associated
 *  instantiating function, with the prototype
 *  BaseObject * createNameOfClass(char const * name, HyPerCol * initData);
 *
 *  Each class's instantiating function should create an object of that class,
 *  with the arguments specifying the object's name and any necessary
 *  initializing data (for most classes, this is the parent HyPerCol.
 *  For HyPerCol, it is the PVInit object).  This way, the class can be
 *  registered with the Factory object by calling
 *  Factory::registerKeyword() with a pointer to the class's instantiating
 *  method.
 *
 *  Created on: Jan 20, 2016
 *      Author: pschultz
 */

#ifndef BASEOBJECT_HPP_
#define BASEOBJECT_HPP_

#include "checkpointing/Checkpointer.hpp"
#include "columns/Messages.hpp"
#include "include/pv_common.h"
#include "observerpattern/Observer.hpp"
#include "utils/PVAlloc.hpp"
#include "utils/PVAssert.hpp"
#include "utils/PVLog.hpp"
#include <memory>

namespace PV {

class HyPerCol;

class BaseObject : public CheckpointerDataInterface {
  public:
   inline char const *getName() const { return name; }
   inline HyPerCol *getParent() const { return parent; }
   char const *getKeyword() const;
   virtual int respond(std::shared_ptr<BaseMessage const> message) override; // TODO: should return
   // enum with values
   // corresponding to
   // PV_SUCCESS,
   // PV_FAILURE,
   // PV_POSTPONE
   virtual ~BaseObject();

   /**
    * Get-method for mInitInfoCommunicatedFlag, which is false on initialization
    * and
    * then becomes true once setInitInfoCommunicatedFlag() is called.
    */
   bool getInitInfoCommunicatedFlag() { return mInitInfoCommunicatedFlag; }

   /**
    * Get-method for mDataStructuresAllocatedFlag, which is false on
    * initialization and
    * then becomes true once setDataStructuresAllocatedFlag() is called.
    */
   bool getDataStructuresAllocatedFlag() { return mDataStructuresAllocatedFlag; }

   /**
    * Get-method for mInitialValuesSetFlag, which is false on initialization and
    * then becomes true once setInitialValuesSetFlag() is called.
    */
   bool getInitialValuesSetFlag() { return mInitialValuesSetFlag; }

#ifdef PV_USE_CUDA
   /**
    * Returns true if the object requires the GPU; false otherwise.
    * HyPerCol will not initialize the GPU unless one of the objects in its
    * hierarchy returns true
    */
   bool isUsingGPU() const { return mUsingGPUFlag; }
#endif // PV_USE_CUDA

  protected:
   BaseObject();
   int initialize(char const *name, HyPerCol *hc);
   int setName(char const *name);
   int setParent(HyPerCol *hc);
   virtual int setDescription();

   int respondCommunicateInitInfo(CommunicateInitInfoMessage const *message);
   int respondAllocateData(AllocateDataMessage const *message);
   int respondRegisterData(RegisterDataMessage<Checkpointer> const *message);
   int respondInitializeState(InitializeStateMessage const *message);
   int respondCopyInitialStateToGPUMessage(CopyInitialStateToGPUMessage const *message);
   int respondCleanup(CleanupMessage const *message);

   virtual int communicateInitInfo() { return PV_SUCCESS; }
   virtual int allocateDataStructures() { return PV_SUCCESS; }
   virtual int initializeState() { return PV_SUCCESS; }
   virtual int readStateFromCheckpoint(Checkpointer *checkpointer) { return PV_SUCCESS; }
   virtual int copyInitialStateToGPU() { return PV_SUCCESS; }
   virtual int cleanup() { return PV_SUCCESS; }

   /**
    * This method sets mInitInfoCommunicatedFlag to true.
    */
   void setInitInfoCommunicatedFlag() { mInitInfoCommunicatedFlag = true; }

   /**
    * This method sets mDataStructuresAllocatedFlag to true.
    */
   void setDataStructuresAllocatedFlag() { mDataStructuresAllocatedFlag = true; }

   /**
    * This method sets the flag returned by getInitialValuesSetFlag to true.
    */
   void setInitialValuesSetFlag() { mInitialValuesSetFlag = true; }

   // Data members
  protected:
   char *name       = nullptr;
   HyPerCol *parent = nullptr; // TODO: eliminate HyPerCol argument to
   // constructor in favor of PVParams argument
   bool mInitInfoCommunicatedFlag    = false;
   bool mDataStructuresAllocatedFlag = false;
   bool mInitialValuesSetFlag        = false;
#ifdef PV_USE_CUDA
   bool mUsingGPUFlag                = false;
#endif // PV_USE_CUDA

  private:
   int initialize_base();
}; // class BaseObject

} // namespace PV

#endif /* BASEOBJECT_HPP_ */
