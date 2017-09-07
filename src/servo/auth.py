import functools
import logging
import jwt
import uuid
import aiohttp.web


log = logging.getLogger(__name__)


def read_context_token(req):
    auth = req.headers.get('authentication')
    if not auth:
        return None

    header, payload = auth.split(' ')
    req['context']['token'] = jwt.decode(payload, req.app['jwt_public_key'])
    log.info('{%s} using existing session' % req['context']['token']['id'])


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

        log.info('{%(client)s} %(method)s %(path)s started' % {
            'client': req['context']['token']['id'],
            'method': req.method,
            'path': req.rel_url
        })
        return await func(req)
    return context_creator
