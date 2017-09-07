import logging
import aiohttp
import aiohttp.web

import servo.auth
import servo.db


log = logging.getLogger(__name__)


@servo.auth.authenticate
async def get(req):
    item_key = req.match_info['key']
    ctx = req['context']
    return aiohttp.web.json_response(await servo.db.read(req.app['database'],
                                                         ctx['token']['id'],
                                                         item_key))


@servo.auth.authenticate
async def post(req):
    item_key = req.match_info['key']
    ctx = req['context']
    return aiohttp.web.json_response(await servo.db.read(req.app['database'],
                                                         ctx['token']['id'],
                                                         item_key))


@servo.auth.authenticate
async def put(req):
    item_key = req.match_info['key']
    ctx = req['context']
    return aiohttp.web.json_response(await servo.db.read(req.app['database'],
                                                         ctx['token']['id'],
                                                         item_key))


@servo.auth.authenticate
async def delete(req):
    # item_key = req.match_info['key']
    # ctx = req['context']
    return aiohttp.web.json_response({})
