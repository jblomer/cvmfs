/**
 * This file is part of the CernVM File System.
 */

#ifndef CVMFS_UPLOAD_S3_H_
#define CVMFS_UPLOAD_S3_H_

#include <string>
#include <pthread.h>

#include "upload_facility.h"
#include "util.h"

namespace webstor {
  class WsConnection;
}

namespace upload {

class S3UploadWorker;

class S3Uploader : public AbstractUploader {
 protected:
  const static std::string kStandardPort;
  const static std::string kMimeType;

  struct WorkerContext {
    std::string host;
    std::string port;
    std::string access_key;
    std::string secret_key;
    std::string bucket;
  };

 public:
  // PolymorphicConstruction methods
  S3Uploader(const SpoolerDefinition &spooler_definition);
  virtual ~S3Uploader();

  static bool WillHandle(const SpoolerDefinition &spooler_definition);


  // AbstractUploader methods
  bool Initialize();
  void TearDown();

  void Upload(const std::string  &local_path,
              const std::string  &remote_path,
              const callback_t   *callback = NULL);
  void Upload(const std::string  &local_path,
              const hash::Any    &content_hash,
              const std::string  &hash_suffix,
              const callback_t   *callback = NULL);

  bool Remove(const std::string &file_to_delete);

  bool Peek(const std::string &path) const;

  void WaitForUpload() const;
  unsigned int GetNumberOfErrors() const;

 protected:
  bool ParseSpoolerDefinition(const SpoolerDefinition &spooler_definition);
  webstor::WsConnection* GetSynchronousS3Connection() const;

 private:
  friend class S3UploadWorker;

  UniquePtr<WorkerContext>                       worker_context_;
  UniquePtr<ConcurrentWorkers<S3UploadWorker> >  concurrent_workers_;

  mutable UniquePtr<webstor::WsConnection>       synchronous_connection_;
  mutable pthread_mutex_t                        synchronous_connection_mutex_;
};

}


#endif /* CVMFS_UPLOAD_S3_H_ */
