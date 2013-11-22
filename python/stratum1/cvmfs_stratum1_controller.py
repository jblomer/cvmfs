import json
import re
import os
import subprocess
from time import sleep
import cvmfs
from cvmfs.repository import LocalRepository, RepositoryNotFound

def _make_json(repo, json_fields):
    json_fields['name'] = repo.fqrn
    return json.dumps(json_fields, indent=4)


def stratum1_status(repo, *args):
    if repo.type != 'stratum1':
        return '400 Bad Request', ''
    spool_dir = repo.read_server_config('CVMFS_SPOOL_DIR')
    output = ''
    try:
        with open(os.path.join(spool_dir, 'snapshot_in_progress')) as snap_file:
            output = _make_json(repo, {'state'      : 'snapshotting',
                                       'start_time' : snap_file.readline()})
    except:
        output = _make_json(repo, {'state' : 'idle'})
    return '200 OK', output


def replicate(repo, *args):
    exec_string  = ["cvmfs_server", "snapshot", repo.fqrn]
    popen_object = None
    try:
        popen_object = subprocess.Popen(exec_string, stdout=subprocess.PIPE,
                                                     stderr=subprocess.PIPE,
                                                     stdin=subprocess.PIPE)
    except OSError, e:
        output = _make_json(repo, {'result'      : 'error',
                                   'description' : str(e)})
        return '503 Service Unavailable', output

    sleep(0.1) # give process some time (might immediately fail/succeed)
    retcode = popen_object.poll()
    if retcode == None or retcode == 0:
        output = _make_json(repo, {'result' : 'ok'})
        return '200 OK', output
    else:
        output = _make_json(repo, {'result'      : 'error',
                                   'return_code' : retcode})
        return '500 Internal Server Error', output


def info(repo, *args):
    output = _make_json(repo, {'type'         : repo.type,
                               'root_catalog' : repo.manifest.root_catalog,
                               'root_hash'    : repo.manifest.root_hash,
                               'ttl'          : repo.manifest.ttl,
                               'revision'     : repo.manifest.revision,
                               'version'      : cvmfs.server_version})
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

