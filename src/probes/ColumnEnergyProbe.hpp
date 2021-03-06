/*
 * ColumnEnergyProbe.hpp
 *
 *  Created on: Aug 12, 2015
 *      Author: pschultz
 */

#ifndef COLUMNENERGYPROBE_HPP_
#define COLUMNENERGYPROBE_HPP_

#include "ColProbe.hpp"

namespace PV {

class BaseProbe;

/**
 * ColumnEnergyProbe assembles several base probes each of which
 * contribute a term to an energy of the entire HyPerCol.
 *
 * At the params file level, define a ColumnEnergyProbe group.  Say,
 * for example that the group is named "total_energy".  Then for each base
 * probe you want to add to the probe, its params should include the line
 *
 * energyProbe = "total_energy";
 *
 * Then optionally specify a coefficient, which defaults to 1.
 *
 * The contribution from this probe to the total energy is
 * coefficient * value
 * where value is the result of calling the BaseProbe's getValues() method.
 *
 * At the C/C++ code level, BaseProbes register themselves to the
 * ColumnEnergyProbe by calling the ColumnEnergyProbe's addTerm() method.
 * Each call to a ColumnEnergyProbe's object that calls addTerm() must
 * have the same getNumValues() value, which becomes the ColumnEnergyProbe's
 * getNumValues() value.
 */

class ColumnEnergyProbe : public ColProbe {
  public:
   /**
    * Public constructor for the ColumnEnergyProbe class.
    */
   ColumnEnergyProbe(const char *probename, HyPerCol *hc);

   /**
    * Destructor for the ColumnEnergyProbe class.
    */
   virtual ~ColumnEnergyProbe();

   /** @brief Adds a probe to the energy calculation.
    * @details Returns PV_SUCCESS if the probe is added successfully.
    * If probe is NULL, the list of terms is unchanged and PV_FAILURE is
    * returned.
    * Nothing prevents a probe from being added more than once.
    * All BaseProbes added to the ColumnEnergyProbe must have the same
    * getNumValues().
    */
   int addTerm(BaseProbe *probe);

   /**
    * Prints the energies to the output stream, formatted as a comma-separated
    * value:
    * "Name of probe",timevalue,index,energy
    * The number of lines printed is equal to getVectorSize(), and index goes
    * from 0 to
    * getVectorSize()-1.
    */
   virtual int outputState(double timevalue);

   virtual int calcValues(double timevalue);

  protected:
   /**
    * The constructor without arguments should be used by derived classes.
    */
   ColumnEnergyProbe();

   /**
    * Reads the parameters and performs initializations that do not
    * depend on other param groups.  It is called by the public constructor
    * and should be called by the initializer of any derived classes.
    */
   int initializeColumnEnergyProbe(const char *probename, HyPerCol *hc);

   virtual int initOutputStream(const char *filename) override;

   virtual int registerData(Checkpointer *checkpointer) override;

   /**
    * Implements the needRecalc method.  Always returns true, in the expectation
    * that the hard work is done by the probes in the constituent energy terms.
    */
   virtual bool needRecalc(double timevalue);

   /**
    * Implementation of referenceUpdateTime().  Since ColumnEnergyProbe updates
    * every timestep, it uses current simulation time.
    */
   virtual double referenceUpdateTime() const;

   virtual int ioParamsFillGroup(enum ParamsIOFlag ioFlag);
   virtual void ioParam_reductionInterval(enum ParamsIOFlag ioFlag);

   size_t numTerms;
   BaseProbe **terms;

  private:
   /**
    * Sets member variables to safe values.  It is called by both the
    * public and protected constructors, and should not otherwise be called.
    */
   int initialize_base();

   int mSkipTimer        = 0;
   int mSkipInterval     = 0;
   double mLastTimeValue = -1;

   // A vector of PrintStreams, one for each batch element.
   // This is a hack, to work around the problem arising with an MPI block with batch dimension > 1.
   // In this situation, several processes have the outputStream defined in BaseProbe writing to
   // the same file, causing collisions.
   //
   // The correct solution to fix this, and other issues, is to overhaul the interface for probes.
   std::vector<PrintStream *> mOutputBatchElements;
}; // end class ColumnEnergyProbe

} // end namespace PV

#endif /* COLUMNENERGYPROBE_HPP_ */
