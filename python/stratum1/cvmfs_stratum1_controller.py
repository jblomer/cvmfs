import json
import re
from cvmfs.repository import LocalRepository, RepositoryNotFound

def status(repo, *args):
    output = json.dumps({'name'  : repo.fqrn,
                         'state' : 'idle',
                         'args'  : repr(args)}, indent=4)
    return '200 OK', output


def replicate(repo, *args):
    output = json.dumps({'result': 'ok'}, indent=4)
    return '200 OK', output


def info(repo, *args):
    output = json.dumps({'result': 'to come'}, indent=4)
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

_RPC_calls = { 'status':    status,
               'replicate': replicate,
               'info':      info }

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

