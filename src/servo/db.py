import os
import logging
import aiopg


log = logging.getLogger(__name__)


async def create_pool(cfg):
    dsn = cfg['servo'].get('database')
    if not dsn:
        dbhost = os.environ.get('DB_HOST')
        dbuser = os.environ.get('DB_USER')
        dbpass = os.environ.get('DB_PASS')
        dbname = os.environ.get('DB_NAME')

        if dbhost and dbuser and dbpass and dbname:
            dsn = 'host=%(h)s dbname=%(n)s ''user=%(u)s password=%(p)s' % {
                'h': dbhost, 'n': dbuser,
                'p': dbpass, 'u': dbname,
            }

    if not dsn:
        raise Exception('No dababase connection details in configuration.')

    log.debug('connecting...')
    return await aiopg.create_pool(dsn)


async def get_items_count(pool):
    async with pool.acquire() as conn:
        async with conn.cursor() as cur:
            await cur.execute("select count(key) from item")
            async for row in cur:
                return row[0]


async def init(pool):
    '''Execute init.sql for given connection pool'''
    here = os.path.abspath(os.path.dirname(__file__))
    async with pool.acquire() as conn:
        async with conn.cursor() as cur:
            with open(os.path.join(here, 'init.sql'), 'r') as init:
                await cur.execute(init.read())


async def read(key):
    log.debug('reading "%s"' % key)
