import os
import logging
import asyncio
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
            dsn = 'host=%(dbhost)s dbname=%(dbname)s user=%(dbuser)s password=%(dbpass)s' % {
                'dbhost': dbhost,
                'dbuser': dbuser,
                'dbpass': dbpass,
                'dbname': dbname,
            }

    if not dsn:
        raise Exception('No dababase connection details found in configuration.')

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
