/*
 * InitVFromFile.hpp
 *
 *  Created on: Oct 26, 2016
 *      Author: pschultz
 */

#ifndef INITVFROMFILE_HPP_
#define INITVFROMFILE_HPP_

#include "BaseInitV.hpp"

namespace PV {

class InitVFromFile : public BaseInitV {
  protected:
   /**
    * List of parameters needed from the InitVFromFile class
    * @name InitVFromFile Parameters
    * @{
    */

   /**
    * @brief VFilename: The path to the file with the initial values.
    * Relative paths are relative to the working directory.
    */
   virtual void ioParam_Vfilename(enum ParamsIOFlag ioFlag);
   /** @} */
  public:
   InitVFromFile(char const *name, HyPerCol *hc);
   virtual ~InitVFromFile();
   virtual int ioParamsFillGroup(enum ParamsIOFlag ioFlag) override;
   virtual int calcV(pvdata_t *V, PVLayerLoc const *loc) override;

  protected:
   InitVFromFile();
   int initialize(char const *name, HyPerCol *hc);
   int checkLoc(const PVLayerLoc *loc, int nx, int ny, int nf, int nxGlobal, int nyGlobal);
   int checkLocValue(int fromParams, int fromFile, const char *field);

  private:
   int initialize_base();

  private:
   char *mVfilename = nullptr;
}; // end class InitVFromFile

} // end namespace PV

#endif /* INITVFROMFILE_HPP_ */