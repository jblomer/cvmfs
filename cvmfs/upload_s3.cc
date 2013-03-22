/**
 * This file is part of the CernVM File System.
 */

#include "upload_s3.h"

#include <cassert>
#include <algorithm>
#include <libs3.h>

#include "util_concurrency.h"

using namespace upload;

const std::string S3Uploader::kStandardPort = "80"; // TODO: ????
const std::string S3Uploader::kMimeType     = "application/x-cvmfs";

namespace upload {

class S3UploadWorker : public ConcurrentWorker<S3UploadWorker> {
 protected:
  typedef S3Uploader::callback_t callback_t;

 private:
  std::string                    full_host_name_;
  S3BucketContext                bucket_context_;
  S3PutProperties                properties_;
  S3PutObjectHandler             put_handler_;

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
  S3UploadWorker(const worker_context *context) {
    full_host_name_ = context->host + ":" + context->port;
    bucket_context_.hostName        = full_host_name_.data();
    bucket_context_.bucketName      = context->bucket.data();
    bucket_context_.protocol        = S3ProtocolHTTP;
    bucket_context_.uriStyle        = S3UriStylePath;
    bucket_context_.accessKeyId     = context->access_key.data();
    bucket_context_.secretAccessKey = context->secret_key.data();
  }


  struct CallbackData {
    CallbackData(      S3UploadWorker    *worker,
                 const MemoryMappedFile  &mmf,
                 const Parameters        &parameters) :
      worker(worker),
      mmf(mmf),
      parameters(parameters),
      bytes_read(0) {}

          S3UploadWorker    *worker;
    const MemoryMappedFile  &mmf;
    const Parameters        &parameters;
          size_t             bytes_read;
  };


  static void completion_callback(      S3Status         status,
                                  const S3ErrorDetails  *error_details,
                                        void            *callback_data) {
    CallbackData *data = (CallbackData*)callback_data;
    if (status == S3StatusOK) {
      LogCvmfs(kLogSpooler, kLogVerboseMsg, "pushed file %s to S3",
               data->parameters.local_path.c_str());
      const S3Uploader::WorkerResults results(data->parameters.local_path,
                                              0,
                                              data->parameters.callback);
      data->worker->master()->JobSuccessful(results);
      return;
    }

    LogCvmfs(kLogSpooler, kLogStderr, "pushing to S3 failed\n"
                                      "  Status:  %d - %s\n"
                                      "  Message: %s\n"
                                      "  Details: %s\n"
                                      "  extras:  %d",
             status, S3_get_status_name(status),
             error_details->message,
             error_details->furtherDetails,
             error_details->extraDetailsCount);

    const S3Uploader::WorkerResults results(data->parameters.local_path,
                                            (int)status,
                                            data->parameters.callback);
    data->worker->master()->JobFailed(results);
  }


  static int data_callback(int buffer_size, char *buffer, void *callback_data) {
    CallbackData *data = (CallbackData*)callback_data;
    size_t bytes_to_copy = std::min((size_t)buffer_size,
                                    data->mmf.size() - data->bytes_read);

    memcpy(buffer,
           data->mmf.buffer() + data->bytes_read,
           bytes_to_copy);
    data->bytes_read += bytes_to_copy;

    return bytes_to_copy;
  }

  void operator()(const Parameters &input) {
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

    CallbackData data(this, mmf, input);
    S3_put_object(&bucket_context_,
                   input.remote_path.data(),
                   mmf.size(),
                   &properties_,
                   NULL,
                   &put_handler_,
                   (void*)&data);

    // Respond() is called in completion callback
  }

  bool Initialize() {
    properties_.contentType                = S3Uploader::kMimeType.data();
    properties_.md5                        = NULL;
    properties_.cacheControl               = NULL;
    properties_.contentDispositionFilename = NULL;
    properties_.contentEncoding            = NULL;
    properties_.expires                    = 0;
    properties_.cannedAcl                  = S3CannedAclPublicRead;
    properties_.metaDataCount              = 0;
    properties_.metaData                   = 0;

    put_handler_.responseHandler.propertiesCallback = NULL;
    put_handler_.responseHandler.completeCallback   = &S3UploadWorker::completion_callback;
    put_handler_.putObjectDataCallback              = &S3UploadWorker::data_callback;

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

  S3Status ret = S3_initialize("", S3_INIT_ALL, "");
  assert (ret == S3StatusOK);

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
  S3_deinitialize();
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
  return true;
}


bool S3Uploader::Peek(const std::string &path) const {
  return false;
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
