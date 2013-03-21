/**
 * This file is part of the CernVM File System.
 */

#include "upload_s3.h"

#include <cassert>
#include <webstor/wsconn.h>

#include "util_concurrency.h"

using namespace upload;

const std::string S3Uploader::kStandardPort = "80"; // TODO: ????
const std::string S3Uploader::kMimeType     = "application/x-cvmfs";

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
               const callback_t   *callback) :
      local_path(local_path), remote_path(remote_path),
      callback(callback) {}

    Parameters() : callback(NULL) {}

    const std::string  local_path;
    const std::string  remote_path;
    const callback_t  *callback;
  };

 public:
  typedef Parameters                 expected_data;
  typedef S3Uploader::WorkerResults  returned_data;
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
      const S3Uploader::WorkerResults results(input.local_path,
                                              1,
                                              input.callback);
      master()->JobFailed(results);
      return;
    }

    webstor::WsPutResponse response;
    connection_->put(bucket_.c_str(),
                     input.remote_path.c_str(),
                     mmf.buffer(),
                     mmf.size(),
                     make_public,
                     server_encryption,
                     S3Uploader::kMimeType.c_str(),
                     &response);

    LogCvmfs(kLogSpooler, kLogVerboseMsg, "S3 etag for %s: %s",
             input.local_path.c_str(), response.etag.c_str());

    const S3Uploader::WorkerResults results(input.local_path,
                                            0,
                                            input.callback);
    master()->JobSuccessful(results);
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
  // Default Spooler Configuration Scheme:
  // <host name>[:port]@<access key>@<secret key>@<bucket name>

  std::vector<std::string>
    config = SplitString(spooler_definition.spooler_configuration, '@');
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

  int retval = pthread_mutex_init(&synchronous_connection_mutex_, NULL);
  assert (retval == 0);

  const unsigned int number_of_cpus = GetNumberOfCpuCores();
  concurrent_workers_ =
    new ConcurrentWorkers<S3UploadWorker>(number_of_cpus,
                                          number_of_cpus * 400,
                                          worker_context_.weak_ref());

  if (! concurrent_workers_->Initialize()) {
    LogCvmfs(kLogSpooler, kLogWarning, "Failed to initialize concurrent "
                                       "workers for S3Uploader.");
    return false;
  }

  concurrent_workers_->RegisterListener(&S3Uploader::UploadWorkerCallback, this);

  return true;
}


void S3Uploader::TearDown() {
  pthread_mutex_destroy(&synchronous_connection_mutex_);
}


void S3Uploader::Upload(const std::string  &local_path,
                        const std::string  &remote_path,
                        const callback_t   *callback) {
  concurrent_workers_->Schedule(
    S3UploadWorker::Parameters(local_path,
                               remote_path,
                               callback));
}


void S3Uploader::Upload(const std::string  &local_path,
                        const hash::Any    &content_hash,
                        const std::string  &hash_suffix,
                        const callback_t   *callback) {
  concurrent_workers_->Schedule(
    S3UploadWorker::Parameters(local_path,
                               MakeCasPath(content_hash, hash_suffix),
                               callback));
}


bool S3Uploader::Remove(const std::string &file_to_delete) {
  MutexLockGuard lock(synchronous_connection_mutex_);

  webstor::WsConnection* connection = GetSynchronousS3Connection();
  assert(connection);

  webstor::WsDelResponse response;
  connection->del(worker_context_->bucket.c_str(),
                  file_to_delete.c_str(),
                  &response);

  return true;
}


bool S3Uploader::Peek(const std::string &path) const {
  MutexLockGuard lock(synchronous_connection_mutex_);

  webstor::WsConnection* connection = GetSynchronousS3Connection();
  assert(connection);

  // TODO: there might be a better way to peek for a file in the S3 API
  //       unfortunately webstor only exposes "get"
  webstor::WsGetResponse response;
  connection->get(worker_context_->bucket.c_str(),
                  path.c_str(),
                  NULL,
                  0,
                  &response);

  return ! response.etag.empty();
}


void S3Uploader::UploadWorkerCallback(const WorkerResults &results) {
  Respond(results.callback, results.return_code, results.local_path);
}


void S3Uploader::WaitForUpload() const {
  AbstractUploader::WaitForUpload();
  concurrent_workers_->WaitForEmptyQueue();
}


void S3Uploader::DisablePrecaching() {
  AbstractUploader::DisablePrecaching();
  concurrent_workers_->EnableDrainoutMode();
}


void S3Uploader::EnablePrecaching() {
  AbstractUploader::EnablePrecaching();
  concurrent_workers_->DisableDrainoutMode();
}


unsigned int S3Uploader::GetNumberOfErrors() const {
  return concurrent_workers_->GetNumberOfFailedJobs();
}


webstor::WsConnection* S3Uploader::GetSynchronousS3Connection() const {
  if (!synchronous_connection_) {
    LogCvmfs(kLogSpooler, kLogVerboseMsg, "lazily creating synchronous S3 "
                                          "connection.");

    webstor::WsConfig configuration = {};

    configuration.accKey   = worker_context_->access_key.c_str();
    configuration.secKey   = worker_context_->secret_key.c_str();
    configuration.host     = worker_context_->host.c_str();
    configuration.port     = worker_context_->port.c_str();
    configuration.storType = webstor::WST_S3;
    configuration.isHttps  = false;

    synchronous_connection_ = new webstor::WsConnection(configuration);
  }

  return synchronous_connection_.weak_ref();
}
