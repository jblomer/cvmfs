import os, sys, re
import cvmfs_stratum1_controller

def application(environ, start_response):
    pattern = re.compile('^/cvmfs/(.*)/control/(.*)$')
    match_result = pattern.search(environ['REQUEST_URI'])

    if not match_result:
        start_response('400 Bad Request', [])
        return []

    repo_fqrn, rpc_uri = match_result.groups()

    return cvmfs_stratum1_controller.main(repo_fqrn, rpc_uri, start_response)

