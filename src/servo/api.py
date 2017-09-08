import logging
import aiohttp
import aiohttp.web

import servo.auth
import servo.db


log = logging.getLogger(__name__)


@servo.auth.authenticate
async def get(req):
    return aiohttp.web.json_response(
        await servo.db.get(req.app['database'],
                           req['context']['token']['id'],
                           req.match_info['key']))


@servo.auth.authenticate
async def post(req):
    item_key = req.match_info['key']
    ctx = req['context']
    if not req.has_body:
        log.error('{%s} request has no body' % ctx['token']['id'])
        raise aiohttp.web.HTTPBadRequest()

    await servo.db.post(req.app['database'],
                        ctx['token']['id'],
                        item_key,
                        ctx['in_type'],
                        req.charset or 'utf-8',
                        req.content)
    return aiohttp.web.json_response({'message': 'Completed Successfuly'},
                                     status=201)


@servo.auth.authenticate
async def put(req):
    return aiohttp.web.json_response({'message': 'Completed Successfuly'})


@servo.auth.authenticate
async def delete(req):
    # item_key = req.match_info['key']
    # ctx = req['context']
    return aiohttp.web.json_response({'message': 'Completed Successfuly'})
