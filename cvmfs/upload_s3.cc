/**
 * This file is part of the CernVM File System.
 */

#include "upload_s3.h"

#include <webstor/wsconn.h>
#include <cassert>

using namespace upload;

const std::string S3Uploader::kStandardPort = "80"; // TODO: ????

namespace upload {

class S3UploadWorker : public ConcurrentWorker<S3UploadWorker> {
 protected:
  typedef S3Uploader::callback_t callback_t;

 private:
  UniquePtr<webstor::WsConnection>  connection_;
  const std::string                 bucket_;

 public:
  struct Parameters {
    Parameters(const std::string  &local_path,
               const std::string  &remote_path,
               const bool          delete_after_upload,
               const callback_t   *callback) :
      local_path(local_path), remote_path(remote_path),
      delete_after_upload(delete_after_upload), callback(callback) {}

    Parameters() : delete_after_upload(false), callback(NULL) {}

    const std::string  local_path;
    const std::string  remote_path;
    const bool         delete_after_upload;
    const callback_t  *callback;
  };

  struct Results {
    Results(const std::string  &local_path,
            const int           return_code,
            const callback_t   *callback) :
      local_path(local_path), return_code(return_code), callback(callback) {}

    const std::string  local_path;
    const int          return_code;
    const callback_t  *callback;
  };

 public:
  typedef Parameters                 expected_data;
  typedef Results                    returned_data;
  typedef S3Uploader::WorkerContext  worker_context;

 public:
  S3UploadWorker(const worker_context *context) :
    bucket_(context->bucket)
  {
    webstor::WsConfig configuration = {};

    configuration.accKey   = context->access_key.c_str();
    configuration.secKey   = context->secret_key.c_str();
    configuration.host     = context->host.c_str();
    configuration.port     = context->port.c_str();
    configuration.storType = webstor::WST_S3;
    configuration.isHttps  = false;

    connection_ = new webstor::WsConnection(configuration);
  }

  void operator()(const Parameters &input) {
    const bool make_public       = true;
    const bool server_encryption = false;

    MemoryMappedFile mmf(input.local_path);
    if (! mmf.Map()) {
      LogCvmfs(kLogSpooler, kLogStderr, "Failed to upload %s",
               input.local_path.c_str());
      master()->JobFailed(Results(input.local_path, 1, input.callback));
      return;
    }

    webstor::WsPutResponse response;
    connection_->put(bucket_.c_str(),
                     input.remote_path.c_str(),
                     mmf.buffer(),
                     mmf.size(),
                     make_public,
                     server_encryption,
                     "application/x-cvmfs",
                     &response);

    master()->JobSuccessful(Results(input.local_path, 0, input.callback));
  }

  bool Initialize() {

    return true;
  }

  void TearDown() {

  }
};

}

//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//

S3Uploader::S3Uploader(const SpoolerDefinition &spooler_definition) :
  AbstractUploader(spooler_definition),
  worker_context_(new S3Uploader::WorkerContext)
{
  if (! ParseSpoolerDefinition(spooler_definition)) {
    abort();
  }

  LogCvmfs(kLogSpooler, kLogVerboseMsg, "Using this S3 Configuration:\n"
                                        "--> Host:       %s\n"
                                        "--> Port:       %s\n"
                                        "--> Access Key: %s\n"
                                        "--> Secret Key: %s\n"
                                        "--> Bucket:     %s",
           worker_context_->host.c_str(),
           worker_context_->port.c_str(),
           worker_context_->access_key.c_str(),
           worker_context_->secret_key.c_str(),
           worker_context_->bucket.c_str());
}


bool S3Uploader::ParseSpoolerDefinition(
                                  const SpoolerDefinition &spooler_definition) {
  std::vector<std::string>
    config = SplitString(spooler_definition.spooler_configuration, ',');
  if (config.size() != 4) {
    LogCvmfs(kLogSpooler, kLogStderr, "Failed to parse S3 spooler definition "
                                      "string: %s",
             spooler_definition.spooler_configuration.c_str());
    return false;
  }

  std::vector<std::string> host = SplitString(config[0], ':');
  if (host.empty() || host.size() > 2) {
    LogCvmfs(kLogSpooler, kLogStderr, "Failed to parse S3 host: %s",
             config[0].c_str());
    return true;
  }

  worker_context_->host       = host[0];
  worker_context_->port       = (host.size() == 2) ? host[1] : kStandardPort;
  worker_context_->access_key = config[1];
  worker_context_->secret_key = config[2];
  worker_context_->bucket     = config[3];
  return true;
}


S3Uploader::~S3Uploader() {
}


bool S3Uploader::WillHandle(const SpoolerDefinition &spooler_definition) {
  return spooler_definition.driver_type == SpoolerDefinition::S3;
}


bool S3Uploader::Initialize() {
  assert (worker_context_);

  const unsigned int number_of_cpus = GetNumberOfCpuCores();
  concurrent_workers_ =
    new ConcurrentWorkers<S3UploadWorker>(number_of_cpus * 5,
                                          number_of_cpus * 400,
                                          worker_context_.weak_ref());

  return true;
}


void S3Uploader::TearDown() {

}


void S3Uploader::Upload(const std::string  &local_path,
                        const std::string  &remote_path,
                        const callback_t   *callback) {
  concurrent_workers_->Schedule(
    S3UploadWorker::Parameters(local_path,
                               remote_path,
                               true,
                               callback));
}


void S3Uploader::Upload(const std::string  &local_path,
                        const hash::Any    &content_hash,
                        const std::string  &hash_suffix,
                        const callback_t   *callback) {
  concurrent_workers_->Schedule(
    S3UploadWorker::Parameters(local_path,
                               "data" + content_hash.MakePath(1,2) + hash_suffix,
                               false,
                               callback));
}


bool S3Uploader::Remove(const std::string &file_to_delete) {
  const bool not_implemented = false;
  assert (not_implemented);

  return false;
}


bool S3Uploader::Peek(const std::string &path) const {
  const bool not_implemented = false;
  assert (not_implemented);

  return false;
}


void S3Uploader::WaitForUpload() const {
  const bool not_implemented = false;
  assert (not_implemented);

}


unsigned int S3Uploader::GetNumberOfErrors() const {
  const bool not_implemented = false;
  assert (not_implemented);

  return 0;
}
