import json
import logging
import aiohttp
import aiohttp.web

import servo.auth
import servo.db


log = logging.getLogger(__name__)


@servo.auth.authenticate
async def get(req):
    item_key = req.match_info['key']
    log.info('{%(client)s} GET %(item)s started' % {
        'client': req['servo']['client'],
        'item': item_key
    })
    return aiohttp.web.json_response(await servo.db.read(item_key))


@servo.auth.authenticate
async def post(req):
    item_key = req.match_info['key']
    log.info('{%(client)s} POST %(item)s started' % {
        'client': req['servo']['client'],
        'item': item_key
    })
    return aiohttp.web.json_response(await servo.db.read(item_key))


@servo.auth.authenticate
async def put(req):
    item_key = req.match_info['key']
    log.info('{%(client)s} PUT %(item)s started' % {
        'client': req['servo']['client'],
        'item': item_key
    })
    return aiohttp.web.json_response(await servo.db.read(item_key))


@servo.auth.authenticate
async def delete(req):
    item_key = req.match_info['key']
    log.info('{%(client)s} DELETE %(item)s started' % {
        'client': req['servo']['client'],
        'item': item_key
    })
    return aiohttp.web.json_response({})
