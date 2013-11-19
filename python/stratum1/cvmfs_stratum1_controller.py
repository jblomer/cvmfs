import json
import re
import os
import cvmfs
from cvmfs.repository import LocalRepository, RepositoryNotFound

def stratum1_status(repo, *args):
    if repo.type != 'stratum1':
        return '400 Bad Request', ''
    spool_dir = repo.read_server_config('CVMFS_SPOOL_DIR')
    output = ''
    try:
        with open(os.path.join(spool_dir, 'snapshot_in_progress')) as snap_file:
            output = json.dumps({'name'       : repo.fqrn,
                                 'state'      : 'snapshotting',
                                 'start time' : snap_file.readline()}, indent=4)
    except:
        output = json.dumps({'name'       : repo.fqrn,
                             'state'      : 'idle'}, indent=4)
    return '200 OK', output


def replicate(repo, *args):
    output = json.dumps({'name'  : repo.fqrn,
                         'result': 'ok'}, indent=4)
    return '200 OK', output


def info(repo, *args):
    output = json.dumps({'name'         : repo.fqrn,
                         'type'         : repo.type,
                         'root_catalog' : repo.manifest.root_catalog,
                         'root_hash'    : repo.manifest.root_hash,
                         'ttl'          : repo.manifest.ttl,
                         'revision'     : repo.manifest.revision,
                         'version'      : cvmfs.server_version}, indent=4)
    return '200 OK', output


def _sanitize_argument_list(argument_list):
    return [ arg for arg in argument_list if arg != '' ]

def _get_repository(fqrn):
    try:
        repo = LocalRepository(fqrn)
        if repo.type != 'stratum1':
            return None
        return repo
    except RepositoryNotFound, e:
        return None

_RPC_calls = { 'stratum1_status' : stratum1_status,
               'replicate'       : replicate,
               'info'            : info }

def main(repo_fqrn, rpc_uri, start_response):
    repo = _get_repository(repo_fqrn)
    if not repo:
        start_response('400 Bad Request', [])
        return []

    uri_tokens = rpc_uri.split('/')
    method     = uri_tokens[0]

    if method not in _RPC_calls:
        start_response('501 Not Implemented', [])
        return []

    args = _sanitize_argument_list(uri_tokens[1:])
    retcode, payload = _RPC_calls[method](repo, args)

    response_headers = [('Content-type',   'application/json'),
                        ('Content-Length', str(len(payload)))]
    start_response(retcode, response_headers)
    return [payload]

