/*
 * io.h
 *
 *  Created on: Oct 24, 2008
 *      Author: rasmussn
 */

#ifndef IO_H_
#define IO_H_

#include "../layers/PVLayer.h"

#ifdef PV_USE_MPI
#  include <mpi.h>
#else
#  include "../include/mpi_stubs.h"
#endif // PV_USE_MPI

#define MIN_BIN_PARAMS  6
#define NUM_BIN_PARAMS (18 + sizeof(double)/sizeof(int))

#define NUM_WGT_EXTRA_PARAMS  6
#define NUM_WGT_PARAMS (NUM_BIN_PARAMS + NUM_WGT_EXTRA_PARAMS)
#define MAX_BIN_PARAMS NUM_WGT_PARAMS

// deprecated, use writeWeights and NUM_WGT_PARAMS
#define NUM_WEIGHT_PARAMS  (MIN_BIN_PARAMS + 3)

#define NUM_PAR_BYTE_PARAMS (NUM_BIN_PARAMS)

#define PV_ERR_FILE_NOT_FOUND 1

#define PV_BYTE_TYPE       1
#define PV_INT_TYPE        2
#define PV_FLOAT_TYPE      3

#define PVP_FILE_TYPE      1 // File type of the *_V_last.pvp and *_A_last.pvp files
#define PVP_ACT_FILE_TYPE  2 // File type of the a%d.pvp and checkpoint files for spiking layers
#define PVP_WGT_FILE_TYPE  3 // File type of the w%d.pvp, w%d_last.pvp, and checkpoint files for non-KernelConn connections
#define PVP_NONSPIKING_ACT_FILE_TYPE  4 // File type of the w%d.pvp, w%d_last.pvp, and checkpoint files for KernelConns
// #define PV_WEIGHTS_FILE_TYPE 3 // Use PVP_WGT_FILE_TYPE instead
#define PVP_KERNEL_FILE_TYPE 5

#define INDEX_HEADER_SIZE  0
#define INDEX_NUM_PARAMS   1
#define INDEX_FILE_TYPE    2
#define INDEX_NX           3
#define INDEX_NY           4
#define INDEX_NF           (MIN_BIN_PARAMS - 1)
#define INDEX_NUM_RECORDS  6
#define INDEX_RECORD_SIZE  7
#define INDEX_DATA_SIZE    8
#define INDEX_DATA_TYPE    9
#define INDEX_NX_PROCS    10
#define INDEX_NY_PROCS    11
#define INDEX_NX_GLOBAL   12
#define INDEX_NY_GLOBAL   13
#define INDEX_KX0         14
#define INDEX_KY0         15
#define INDEX_NB          16
#define INDEX_NBANDS      17
#define INDEX_TIME        18

// these are extra parameters used by weight files
//
#define INDEX_WGT_NXP        0
#define INDEX_WGT_NYP        1
#define INDEX_WGT_NFP        2
#define INDEX_WGT_MIN        3
#define INDEX_WGT_MAX        4
#define INDEX_WGT_NUMPATCHES 5

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

int pv_getopt_int(int argc, char * argv[], const char * opt, int *   iVal);
int pv_getopt_str(int argc, char * argv[], const char * opt, char ** sVal);
int pv_getopt_unsigned_long(int argc, char * argv[], const char * opt, unsigned long * ulVal);

int readFile(const char * filename, float * buf, int * nx, int * ny);
#ifdef OBSOLETE // Marked obsolete Sept 27, 2011.  These functions have been superseded and no other functions call them.
int scatterReadBuf(PVLayer * l, float * globalBuf, float * localBuf, MPI_Comm comm);
int scatterReadFile(const char * filename, PVLayer * l, float * buf, MPI_Comm comm);
int gatherWriteFile(const char * filename, PVLayer * l, float * ibuf, MPI_Comm comm);
int pv_tiff_write_patch(FILE * fd, PVPatch * patch);
int pv_tiff_write_cube(const char * filename, PVLayerCube * cube, int nx, int ny, int nf);
int printStats(pvdata_t * buf, int nItems, char * msg);
int pv_dump(const char * output_path, const char * filename, int append, pvdata_t * I, int nx, int ny, int nf);
int pv_dump_sparse(const char * output_path, const char * filename, int append, pvdata_t * I, int nx, int ny, int nf);
int pv_write_patch(FILE * fp, int numTotal, float minVal, float maxVal, PVPatch * p);
int pv_write_patches(const char * output_path, const char * filename, int append,
                     int nx, int ny, int nf, float minVal, float maxVal,
                     int numPatches, PVPatch ** patches);
int pv_read_patches(FILE *fp, int nxp, int nyp, int nfp, float minVal, float maxVal,
                    int numPatches, PVPatch ** patches);
int log_parameters(int n_time_steps, char * input_filename);
#endif // OBSOLETE
int pv_text_write_patch(FILE * fd, PVPatch * patch);
int pv_center_image(float * V, int nx0, int ny0, int nx, int ny);

FILE * pv_open_binary(const char * filename, int * numParams, int * type, int * nx, int * ny, int * nf);
int    pv_read_binary_params(FILE * fp, int numParams, int params[]);
int    pv_close_binary(FILE * fp);
size_t pv_read_binary_record(FILE * fp, pvdata_t * buf, int nItems);

int parse_options(int argc, char * argv[], char ** output_path, char ** input_file,
                  char ** param_file, int * n_time_steps, int * opencl_device, unsigned long * random_seed, char ** working_dir);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* IO_H_ */
