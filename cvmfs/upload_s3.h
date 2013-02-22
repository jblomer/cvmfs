/**
 * This file is part of the CernVM File System.
 */

#ifndef CVMFS_UPLOAD_S3_H_
#define CVMFS_UPLOAD_S3_H_

#include <string>

#include "upload_facility.h"
#include "util.h"

namespace webstor {
  class WsConnection;
}

namespace upload {

class S3Uploader : public AbstractUploader {
 protected:
  const static std::string kStandardPort;

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

 private:
  std::string                       host_;
  std::string                       port_;
  std::string                       access_key_;
  std::string                       secret_key_;

  UniquePtr<webstor::WsConnection>  connection_;
};

}


#endif /* CVMFS_UPLOAD_S3_H_ */
