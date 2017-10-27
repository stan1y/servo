import asyncio
import aiohttp
import aiohttp.web
import datetime
import logging
import ssl

import servo.auth
import servo.api
import servo.db
import servo.err

log = logging.getLogger(__name__)


@servo.auth.authenticate
async def stats(req):
    ctx = req['context']
    return aiohttp.web.json_response({
        'client': ctx['token']['id'],
        'session_ttl': ctx['token']['ttl'],
        'last_read': int(datetime.datetime.utcnow().timestamp()),
        'last_write': int(datetime.datetime.utcnow().timestamp())
    })


async def start_tasks(app):
    pass


async def stop_tasks(app):
    pass


async def connect_db(app):
    '''Connect and retry connections untill run out of attempts.'''
    max_attempts = int(app['config'].get(
        'database', 'attempts', fallback=5))
    wait_time = float(app['config'].get(
        'database', 'attempt_wait', fallback=5.0))
    attempts = max_attempts

    while attempts:
        try:
            pool = await servo.db.create_pool(app['config'])
            app['database'] = pool
            count = await servo.db.get_items_count(app['database'])
            log.debug('found items table with %d records' % count)
            return

        except Exception as ex:
            log.error('failed to connect to database.')
            await asyncio.sleep(wait_time, loop=app.loop)
            attempts = attempts - 1
            log.debug('retrying connection %d...' % (max_attempts - attempts))
    raise servo.err.ConfigurationError(
        'Failed to connect to database after %d attempts' %
        max_attempts)


def create_app(cfg):
    app = aiohttp.web.Application()
    app['config'] = cfg
    app['jwt_private_key'] = servo.auth.load_key(cfg.get('jwt', 'private',
                                                 fallback=None))
    app['jwt_public_key'] = servo.auth.load_key(cfg.get('jwt', 'public',
                                                fallback=None))

    app.on_startup.append(connect_db)
    app.on_startup.append(start_tasks)
    app.on_cleanup.append(stop_tasks)

    app.router.add_get('/',         stats)
    app.router.add_get('/{key}',    servo.api.get)
    app.router.add_post('/{key}',   servo.api.post)
    app.router.add_put('/{key}',    servo.api.put)
    app.router.add_delete('/{key}', servo.api.delete)
    return app


def create_ssl_context(cfg):
    cert = cfg.get('ssl', 'cert', fallback=None)
    key = cfg.get('ssl', 'key', fallback=None)
    if cert and key:
        ctx = ssl.create_default_context()
        log.debug('setting up SSL with: %s, %s' % (cert, key))
        ctx.load_cert_chain(cert, key)
        return ctx
    return None
