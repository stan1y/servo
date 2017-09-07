import functools
import json
import aiohttp
import aiohttp.web
import logging


log = logging.getLogger(__name__)


def authenticate(func):
    @functools.wraps(func)
    async def wrapper(req):
        req['servo'] = {
            'client': 'XXXXXX',
            'token': 'TOKEN',
            'in_content_type': 'text/plain',
            'out_content_type': 'text/plain'
        }
        log.info('{%s} authenticated "%s"' % (
            req['servo']['client'],
            func.__name__
        ))

        return await func(req)
    return wrapper
