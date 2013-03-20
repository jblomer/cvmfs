/**
 * This file is part of the CernVM File System.
 */

#include "upload_facility.h"

#include "upload_local.h"
#include "upload_riak.h"
#include "upload_s3.h"

using namespace upload;

void AbstractUploader::RegisterPlugins() {
  RegisterPlugin<LocalUploader>();
  RegisterPlugin<RiakUploader>();
  RegisterPlugin<S3Uploader>();
}


AbstractUploader::AbstractUploader(const SpoolerDefinition& spooler_definition) :
  spooler_definition_(spooler_definition)
{}


bool AbstractUploader::Initialize() {
  return true;
}


void AbstractUploader::TearDown() {}


void AbstractUploader::WaitForUpload() const {}


void AbstractUploader::DisablePrecaching() {}


void AbstractUploader::EnablePrecaching() {}


void AbstractUploader::Respond(const callback_t  *callback,
                               const int          return_code,
                               const std::string  local_path) {
  if (callback == NULL) {
    return;
  }

  const UploaderResults result(return_code, local_path);
  (*callback)(result);
  delete callback;
}


const std::string AbstractUploader::MakeCasPath(
                                        const hash::Any    &content_hash,
                                        const std::string  &hash_suffix) const {
  return "data" + content_hash.MakePath(1,2) + hash_suffix;
}
