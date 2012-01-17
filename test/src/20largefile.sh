
cvmfs_test_name="Load large file"

do_tests() {
  logfile=$1

  extract_local_repo largefile || return 1
  setup_local none || return 1
  service cvmfs restartclean >> $logfile 2>&1 || return 1

  ls /cvmfs/127.0.0.1 >> $logfile 2>&1 || return 2
  ls -lah /cvmfs/127.0.0.1/2.4G >> $logfile 2>&1 || return 2

  # Download and open large file
  head -c1 /cvmfs/127.0.0.1/2.4G >> $logfile 2>&1 || return 3
  # Open large file from cach
  head -c1 /cvmfs/127.0.0.1/2.4G >> $logfile 2>&1 || return 4

  return 0
}


cvmfs_run_test() {
  logfile=$1

  do_tests $logfile
  RETVAL=$?

  cleanup_local

  return $RETVAL
}

