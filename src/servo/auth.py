import os
import functools
import logging
import jwt
import uuid
import aiohttp.web

import servo.err


log = logging.getLogger(__name__)


def load_key(pem_file):
    log.debug('loading key from %s' % pem_file)
    if pem_file and os.path.exists(pem_file):
        with open(pem_file, 'rb') as f:
            return jwt.jwk_from_pem(f.read())
    raise servo.err.ConfigurationError("Failed to load JWT key")


def read_context_token(req):
    '''Read authentication header, parse JWT and setup context'''
    auth = req.headers.get('authentication')
    if not auth:
        return None

    scheme, token = auth.split(' ')
    if scheme.lower() != 'bearer':
        log.error('unexpected authentication scheme: %s' % scheme)
        raise aiohttp.web.HttpProcessingError(scheme)

    pub_key = req.app['jwt_public_key']
    req['context']['token'] = jwt.JWT().decode(token,
                                               pub_key)
    log.info('{%s} using existing session' % req['context']['token']['id'])


def write_context_token(req, resp):
    '''Write context token to authentication header'''
    priv_key = req.app['jwt_private_key']
    alg = req.app['config'].get('jwt', 'alg', fallback='RS256')
    headers = {'iss': req.headers.get('host')}
    resp.headers['authentication'] = jwt.JWT().encode(req['context']['token'],
                                                      priv_key, alg, headers)


def init_context(req):
    req['context']['token'] = {
        'id': str(uuid.uuid4()),
        'ttl': req.app['config'].get('session', 'ttl', fallback=300)
    }
    log.info('{%s} created new session' % req['context']['token']['id'])


def authenticate(func):
    @functools.wraps(func)
    async def context_creator(req):
        '''Creates authentication context by checking or emitting jwt'''
        public_mode = req.app['config'].getboolean('servo', 'public_mode',
                                                   fallback=True)
        allow_origin = req.app['config'].get('filter', 'origin', fallback=None)
        allow_ip = req.app['config'].get('filter', 'ip_address', fallback=None)

        if allow_origin and not public_mode:
            origin = req.headers.get('origin')
            if origin != allow_origin:
                log.error('forbid origin "%s"' % origin)
                raise aiohttp.web.HTTPForbidden()

        if allow_ip:
            peer = req.transport.get_extra_info('peername')
            if peer is not None:
                peer_ip, peer_port = peer
                if peer_ip != allow_ip:
                    log.error('forbid client ip "%s"' % peer_ip)
                    raise aiohttp.web.HTTPForbidden()

        # read or initialize authentication context
        req['context'] = {
            'token': None,
            'in_content_type': 'text/plain',
            'out_content_type': 'text/plain'
        }
        if not read_context_token(req):
            init_context(req)

        # log call into request handler
        log.info('{%(client)s} >> %(method)s %(path)s started' % {
            'client': req['context']['token']['id'],
            'method': req.method,
            'path': req.rel_url
        })
        resp = await func(req)
        write_context_token(req, resp)
        log.info("{%(client)s} << %(method)s %(path)s completed => %(s)d" % {
            'client': req['context']['token']['id'],
            'method': req.method,
            'path': req.rel_url,
            's': resp.status
        })
        return resp
    return context_creator
