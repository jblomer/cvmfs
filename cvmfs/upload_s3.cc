/**
 * This file is part of the CernVM File System.
 */

#include "upload_s3.h"

#include <webstor/wsconn.h>

using namespace upload;

const std::string S3Uploader::kStandardPort = "80"; // TODO: ????

S3Uploader::S3Uploader(const SpoolerDefinition &spooler_definition) :
  AbstractUploader(spooler_definition),
  connection_(NULL)
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
  webstor::WsConfig configuration = {};

  configuration.accKey   = access_key_.c_str();
  configuration.secKey   = secret_key_.c_str();
  configuration.host     = host_.c_str();
  configuration.port     = port_.c_str();
  configuration.storType = webstor::WST_S3;
  configuration.isHttps  = false;

  connection_ = new webstor::WsConnection(configuration);
  return true;
}


void S3Uploader::TearDown() {

}


void S3Uploader::Upload(const std::string  &local_path,
                        const std::string  &remote_path,
                        const callback_t   *callback) {
  const bool not_implemented = false;
  assert (not_implemented);

}


void S3Uploader::Upload(const std::string  &local_path,
                        const hash::Any    &content_hash,
                        const std::string  &hash_suffix,
                        const callback_t   *callback) {
  const bool not_implemented = false;
  assert (not_implemented);

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
