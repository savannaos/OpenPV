#ifndef __CHECKPOINTABLEFILESTREAM_HPP__
#define __CHECKPOINTABLEFILESTREAM_HPP__

#include "Checkpointer.hpp"
#include "io/FileStream.hpp"

#include <string>

using std::string;

namespace PV {

class CheckpointableFileStream : public FileStream, public CheckpointerDataInterface {

  public:
   /**
    * Constructor for CheckpointableFileStream. Opens a file for reading and
    * writing at the path indicated, and registers its file positions with the
    * given checkpointer. The path must be a relative path; it is
    * relative to the checkpointer's OutputPath directory.
    * If newFile is true, the file is created (clobbering the file if it
    * already exists). If newFile is false and the file does not exist, a
    * warning is issued and the file is created.
    * A CheckpointableFileStream can only be instantiated by the root process
    * of the Checkpointer's MPIBlock; all other processes generate a fatal
    * error. objName is the object name used when registering the file
    * positions with the checkpointer.
    * verifyWrites has the same meaning as in the FileStream constructor.
    */
   CheckpointableFileStream(
         string const &path,
         bool newFile,
         Checkpointer *checkpointer,
         string const &objName,
         bool verifyWrites);

   /**
    * This constructor is identical to the previous constructor, except that
    * the checkpointer's verifyWrites flag is used in place of an explicit
    * argument.
    */
   CheckpointableFileStream(
         string const &path,
         bool newFile,
         Checkpointer *checkpointer,
         string const &objName);
   virtual int respond(std::shared_ptr<BaseMessage const> message) override;
   virtual void write(void const *data, long length);
   virtual void read(void *data, long length);
   virtual void setOutPos(long pos, bool fromBeginning);
   virtual void setInPos(long pos, bool fromBeginning);

   /**
    * Returns the string for the path that would be generated if a CheckpointableFileStream
    * is instantiated with the given arguments. This function is called during instantiation,
    * and is also useful if the path is needed before creating the file; i.e. verifying that
    * the file exists, or creating it.
    */
   static string makeOutputPathFilename(Checkpointer *checkpointer, string const &path);

  private:
   void initialize(
         string const &path,
         bool newFile,
         Checkpointer *checkpointer,
         string const &objName,
         bool verifyWrites);
   void setDescription();
   virtual int registerData(Checkpointer *checkpointer);
   int respondProcessCheckpointRead(ProcessCheckpointReadMessage const *message);
   void syncFilePos();
   void updateFilePos();
   long mFileReadPos  = 0;
   long mFileWritePos = 0;
   string mObjName; // Used for CheckpointerDataInterface
};
}

#endif
