/**
 * This file is part of the CernVM File System.
 */

#include "upload_s3.h"

using namespace upload;

S3Uploader::S3Uploader(const SpoolerDefinition &spooler_definition) :
  AbstractUploader(spooler_definition)
{

}


S3Uploader::~S3Uploader() {

}


bool S3Uploader::WillHandle(const SpoolerDefinition &spooler_definition) {
  return spooler_definition.driver_type == SpoolerDefinition::S3;
}


bool S3Uploader::Initialize() {

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
