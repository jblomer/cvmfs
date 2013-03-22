#include <cassert>
#include <iostream>

#include "upload_spooler_definition.h"
#include "upload_facility.h"
#include "upload_file_processor.h"
#include "upload_file_chunker.h"
#include "util.h"

using namespace upload;

int main() {
  SpoolerDefinition sd("s3,/ramdisk,olhw-s3.cern.ch:5080@C6FA083F94DDCB51B95A@eHruFhsc6MVIptQ1uunMMhVneZ0AAAE8lN3LUm8W@cvmfs");
  UniquePtr<AbstractUploader> upl;
  upl = AbstractUploader::Construct(sd);

  upl->Upload("cvmfs/debug", "data/debug");

  upl->WaitForUpload();
}
