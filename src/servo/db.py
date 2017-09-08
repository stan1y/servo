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


async def get(req):
    '''Read one of the available values for given pair of client and key'''
    pool = req.app['database']
    client = req['context']['token']['id']
    key = req.match_info['key']

    async with pool.acquire() as conn:
        async with conn.cursor() as cur:
            await cur.execute(asset_get_sql, {
                'client': client,
                'key': key
            })
            val = await cur.fetchone()
            if not val:
                raise aiohttp.web.HTTPNotFound()
            str_val, json_val, blob_val = val
            if str_val:
                return str_val
            if json_val:
                return json_val
            if blob_val:
                return base64.b64encode(blob_val)


async def get_request_data(req):
    '''Read request data and produce item payloads'''
    str_val = None
    json_val = None
    blob_val = None
    stype = req['context']['in_type']
    if stype == 'text/plain' or stype == 'text/html':
        str_val = await req.text()
    elif stype == 'application/json':
        json_val = json.dumps(await req.json())
    elif stype == 'multipart/form-data':
        reader = await req.multipart()
        afile = await reader.next()
        size = 0
        blob_val = b''
        while True:
            chunk = await afile.read_chunk()  # 8192 bytes by default.
            if not chunk:
                break
            size += len(chunk)
            blob_val += chunk
        blob_val = blob_val.decode(req.charset or 'utf-8')

    return str_val, json_val, blob_val


async def post(req):
    '''Create new item owned by client with key, type and data'''
    pool = req.app['database']
    client = req['context']['token']['id']
    key = req.match_info['key']

    str_val, json_val, blob_val = await get_request_data(req)
    async with pool.acquire() as conn:
        async with conn.cursor() as cur:
            await cur.execute(asset_post_sql, {
                'client': client,
                'key': key,
                'str_val': str_val,
                'json_val': json_val,
                'blob_val': blob_val
            })


async def put(req):
    '''Modify item owned by client with key, type and data'''
    pool = req.app['database']
    client = req['context']['token']['id']
    key = req.match_info['key']

    str_val, json_val, blob_val = await get_request_data(req)
    async with pool.acquire() as conn:
        async with conn.cursor() as cur:
            await cur.execute(asset_put_sql, {
                'client': client,
                'key': key,
                'str_val': str_val,
                'json_val': json_val,
                'blob_val': blob_val
            })
