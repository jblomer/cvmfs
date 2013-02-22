/**
 * This file is part of the CernVM File System.
 */

#include "upload_s3.h"

#include <webstor/wsconn.h>

using namespace upload;

const std::string S3Uploader::kStandardPort = "80"; // TODO: ????

namespace upload {

class S3UploadWorker : public ConcurrentWorker<S3UploadWorker> {
 private:
  UniquePtr<webstor::WsConnection>  connection_;

 protected:
  typedef S3Uploader::callback_t callback_t;

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
  S3UploadWorker(const worker_context *context) {
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
  AbstractUploader(spooler_definition)
{
  if (! ParseSpoolerDefinition(spooler_definition)) {
    abort();
  }

  LogCvmfs(kLogSpooler, kLogVerboseMsg, "Using this S3 Configuration:\n"
                                        "--> Host: %s\n"
                                        "--> Port: %s\n"
                                        "--> Access Key: %s\n"
                                        "--> Secret Key: %s",
           host_.c_str(),
           port_.c_str(),
           access_key_.c_str(),
           secret_key_.c_str());
}


bool S3Uploader::ParseSpoolerDefinition(
                                  const SpoolerDefinition &spooler_definition) {
  std::vector<std::string>
    config = SplitString(spooler_definition.spooler_configuration, ',');
  if (config.size() != 3) {
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

  host_       = host[0];
  port_       = (host.size() == 2) ? host[1] : kStandardPort;
  access_key_ = config[1];
  secret_key_ = config[2];
  return true;
}


S3Uploader::~S3Uploader() {
}


bool S3Uploader::WillHandle(const SpoolerDefinition &spooler_definition) {
  return spooler_definition.driver_type == SpoolerDefinition::S3;
}


bool S3Uploader::Initialize() {
  worker_context_     = new WorkerContext(host_,
                                          port_,
                                          access_key_,
                                          secret_key_);

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
