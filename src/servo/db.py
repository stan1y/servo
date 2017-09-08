import os
import json
import logging
import aiopg
import aiohttp.web
import base64

import servo

log = logging.getLogger(__name__)

# Load sql files on start
here = os.path.abspath(os.path.dirname(__file__))
with open(os.path.join(here, 'sql', 'init.sql'), 'r') as f:
    asset_init_sql = f.read()
with open(os.path.join(here, 'sql', 'get.sql'), 'r') as f:
    asset_get_sql = f.read()
with open(os.path.join(here, 'sql', 'post.sql'), 'r') as f:
    asset_post_sql = f.read()
with open(os.path.join(here, 'sql', 'put.sql'), 'r') as f:
    asset_put_sql = f.read()
with open(os.path.join(here, 'sql', 'delete.sql'), 'r') as f:
    asset_delete_sql = f.read()


async def create_pool(cfg):
    '''Create new PostgreSQL connections pool'''
    dsn = cfg['servo'].get('database')
    if not dsn:
        dbhost = os.environ.get('DB_HOST')
        dbuser = os.environ.get('DB_USER')
        dbpass = os.environ.get('DB_PASS')
        dbname = os.environ.get('DB_NAME')

        if dbhost and dbuser and dbpass and dbname:
            dsn = 'host=%(h)s dbname=%(n)s ''user=%(u)s password=%(p)s' % {
                'h': dbhost, 'u': dbuser,
                'p': dbpass, 'n': dbname,
            }

    if not dsn:
        raise Exception('No dababase connection details in configuration.')

    log.debug('connecting...')
    return await aiopg.create_pool(dsn)


async def get_items_count(pool):
    '''Verify item table exists and return number of records'''
    async with pool.acquire() as conn:
        async with conn.cursor() as cur:
            await cur.execute('select count(key) from item')
            async for row in cur:
                return row[0]


async def init(pool):
    '''Execute init.sql for given connection pool'''
    async with pool.acquire() as conn:
        async with conn.cursor() as cur:
            await cur.execute(asset_init_sql)


async def get(pool, client, key):
    '''Read one of the available values for given pair of client and key'''
    async with pool.acquire() as conn:
        async with conn.cursor() as cur:
            await cur.execute(asset_get_sql, [client, key])
            val = await cur.fetchone()
            if not val:
                raise aiohttp.web.HTTPNotFound()
            str_val, json_val, blob_val = val
            if str_val:
                log.debug('read string from "%s"' % key)
                return str_val
            if json_val:
                log.debug('read json from "%s"' % key)
                return json_val
            if blob_val:
                log.debug('read blob from "%s"' % key)
                return base64.encode(blob_val)


async def post(pool, client, key, stype, charset, content):
    '''Create new item owner by client with key, type and data'''
    data = []
    async for line in content:
        data.append(line.decode(charset))
    str_val = None
    json_val = None
    blob_val = None
    if stype == servo.TYPE_STRING or stype == servo.TYPE_HTML:
        str_val = ''.join(data)
    elif stype == servo.TYPE_JSON:
        try:
            data = json.loads(''.join(data))
            json_val = json.dumps(data)
        except Exception as ex:
            log.error('{%s} invalid json received' % client)
            raise aiohttp.web.HTTPBadRequest()
    elif stype == servo.TYPE_BLOB:
        log.error('blobs not supported yet')
        raise aiohttp.web.HTTPNotImplemented()

    async with pool.acquire() as conn:
        async with conn.cursor() as cur:
            await cur.execute(asset_post_sql, [
                client, key,
                str_val, json_val, blob_val
            ])
