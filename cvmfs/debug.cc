#include <cassert>
#include <iostream>

#include "upload_spooler_definition.h"
#include "upload_facility.h"
#include "upload_file_processor.h"
#include "upload_file_chunker.h"
#include "util.h"

using namespace upload;

UniquePtr<ConcurrentWorkers<FileProcessor> > concurrent_processing;

void MyCallback(const FileProcessor::Results &data) {

}

void Schedule(const std::string &path) {
  const FileProcessor::Parameters params(path, true);
  concurrent_processing->Schedule(params);
}

void Recurse(const std::string &dir_path) {
  DIR *dir;
  struct dirent *ent;
  dir = opendir(dir_path.c_str());
  assert (dir != NULL);

  /* print all the files and directories within directory */
  while ((ent = readdir(dir)) != NULL) {
    const std::string path = dir_path + "/" + ent->d_name;

    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
      continue;
    }

    if (ent->d_type & DT_DIR) {
      Recurse(path);
    } else {
      Schedule(path);
    }
  }
  closedir(dir);
}

int main() {

  // SpoolerDefinition sd("s3,/ramdisk,10.40.53.157:5080@C6FA083F94DDCB51B95A@eHruFhsc6MVIptQ1uunMMhVneZ0AAAE8lN3LUm8W@cvmfs",
  //                      true, 4000000, 8000000, 16000000);
  // SpoolerDefinition sd("riak,/ramdisk,http://cernvmbl005:8098/riak/cvmfs@http://cernvmbl006:8098/riak/cvmfs@http://cernvmbl007:8098/riak/cvmfs@http://cernvmbl008:8098/riak/cvmfs@http://cernvmbl009:8098/riak/cvmfs",
  //                      true, 4000000, 8000000, 16000000);
  SpoolerDefinition sd("local,/ramdisk/data/txn,/ramdisk/",
                       true, 4000000, 8000000, 16000000);

  assert (sd.IsValid());
  UniquePtr<AbstractUploader> uploader;
  uploader = AbstractUploader::Construct(sd);
  if (!uploader) {
    std::cout << "failed to init upload" << std::endl;
    return false;
  }

  UniquePtr<FileProcessor::worker_context> concurrent_processing_context;
  concurrent_processing_context =
    new FileProcessor::worker_context(sd.temporary_path,
                                      sd.use_file_chunking,
                                      uploader.weak_ref());

  const unsigned int number_of_cpus = GetNumberOfCpuCores();
  concurrent_processing =
    new ConcurrentWorkers<FileProcessor>(number_of_cpus,
                                         number_of_cpus * 500, // TODO: magic number (?)
                                         concurrent_processing_context.weak_ref());

  assert(concurrent_processing);
  concurrent_processing->RegisterListener(&MyCallback);

  // initialize the file processor environment
  if (! concurrent_processing->Initialize()) {
    std::cout << "failed to init processing" << std::endl;
    return false;
  }

  Recurse("/mnt/benchmark_repo/extracted");

  concurrent_processing->WaitForEmptyQueue();
  uploader->WaitForUpload();

  return 0;
}
