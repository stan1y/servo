import os
import functools
import logging
import jwt
import uuid
import aiohttp.web

import servo
import servo.err


log = logging.getLogger(__name__)


def load_key(pem_file):
    log.debug('loading key from %s' % pem_file)
    if pem_file and os.path.exists(pem_file):
        with open(pem_file, 'rb') as f:
            return jwt.jwk_from_pem(f.read())
    raise servo.err.ConfigurationError("Failed to load JWT key")


def content_type_to_servo(ctype):
    if 'text/plain' in ctype:
        return servo.TYPE_STRING
    if 'application/json' in ctype:
        return servo.TYPE_JSON
    if 'multipart/form-data' in ctype:
        return servo.TYPE_BLOB
    if 'text/html' in ctype:
        return servo.TYPE_HTML
    return None


def read_out_type(req):
    '''Read accept headers and convert to servo item type'''
    accepts = req.headers.getall('accept', ['text/plain'])
    for ctype in accepts:
        stype = content_type_to_servo(ctype)
        if stype:
            return stype
    log.error('unsupported accepted types: %s' % (
        ', '.join(accepts)))
    raise aiohttp.web.HTTPBadRequest()


def read_in_type(req):
    '''Read content-type headers and convert to servo item type'''
    types = req.headers.getall('content-type', ['text/plain'])
    for ctype in types:
        stype = content_type_to_servo(ctype)
        if stype:
            return stype
    log.error('unsupported content types: %s' % (
        ', '.join(types)))
    raise aiohttp.web.HTTPBadRequest()


def read_context_token(req):
    '''Read authorization header, parse JWT and setup context'''
    auth = req.headers.get('authorization')
    if not auth:
        return None

    scheme, token = auth.split(' ')
    if scheme.lower() != 'bearer':
        log.error('unexpected authorization scheme "%s"' % scheme)
        raise aiohttp.web.HttpProcessingError(scheme)

    pub_key = req.app['jwt_public_key']
    return jwt.JWT().decode(token, pub_key)


def write_context_token(req, resp):
    '''Write context token to authorization header'''
    priv_key = req.app['jwt_private_key']
    alg = req.app['config'].get('jwt', 'alg', fallback='RS256')
    headers = {'iss': req.headers.get('host')}
    encoded = jwt.JWT().encode(req['context']['token'],
                               priv_key, alg, headers)
    resp.headers['authorization'] = 'bearer %s' % encoded


def init_context(req):
    req['context']['token'] = {
        'id': str(uuid.uuid4()),
        'iss': req.headers.get('host'),
        'ttl': req.app['config'].get('session', 'ttl', fallback=300)
    }
    log.debug('{%s} allocated new client session' %
              req['context']['token']['id'])


def authenticate(func):
    @functools.wraps(func)
    async def context_creator(req):
        '''Creates authorization context by checking or emitting jwt'''
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

        # read or initialize authorization context
        req['context'] = {
            'token': read_context_token(req),
            'in_type': read_in_type(req),
            'out_type': read_out_type(req)
        }
        if not req['context']['token']:
            init_context(req)

        # log call into request handler
        log.info('{%(client)s} >> %(method)s %(path)s started' % {
            'client': req['context']['token']['id'],
            'method': req.method,
            'path': req.rel_url
        })
        log.debug('{%s} content types, in: %s, out: %s' % (
            req['context']['token']['id'],
            servo.stype2str(req['context']['in_type']),
            servo.stype2str(req['context']['out_type']),
        ))
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
