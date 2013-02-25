#include <cassert>
#include <iostream>

#include "upload_spooler_definition.h"
#include "upload_facility.h"
#include "util.h"

using namespace upload;


int main() {

  SpoolerDefinition sd("s3,/tmp,olhw-s3.cern.ch:5080@C6FA083F94DDCB51B95A@eHruFhsc6MVIptQ1uunMMhVneZ0AAAE8lN3LUm8W@cvmfs");
  assert (sd.IsValid());

  UniquePtr<AbstractUploader> upl(AbstractUploader::Construct(sd));
  assert (upl);

  upl->Upload("Makefile", "data/Makefile");
  upl->Upload("cmake_install.cmake", "data/cmake_install.cmake");
  upl->Upload("CMakeCache.txt", "data/CMakeCache.txt");
  upl->Upload("install_manifest.txt", "data/install_manifest.txt");
  upl->Upload("cvmfs_config.h", "data/cvmfs_config.h");
  // upl->Upload("/mnt/atlas-condb/cond12_data.000002.lar.COND._0008.pool.root", "data/cond12_data.000002.lar.COND._0008.pool.root");
  // upl->Upload("/mnt/atlas-condb/cond12_data.000002.lar.COND._0010.pool.root", "data/cond12_data.000002.lar.COND._0010.pool.root");

  upl->WaitForUpload();

  upl->Remove("data/cvmfs_config.h");
  upl->Remove("data/install_manifest.txt");

  bool res = upl->Peek("data/CMakeCache.txt");

  std::cout << (res ? "jep" : "nope") << std::endl;

  return 0;

}
