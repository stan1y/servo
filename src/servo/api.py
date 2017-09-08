import logging
import aiohttp
import aiohttp.web

import servo.auth
import servo.db


log = logging.getLogger(__name__)


@servo.auth.authenticate
async def get(req):
    return aiohttp.web.json_response(await servo.db.get(req))


@servo.auth.authenticate
async def post(req):
    if not req.has_body:
        log.error('{%s} POST has no body' % req['context']['token']['id'])
        raise aiohttp.web.HTTPBadRequest()
    await servo.db.post(req)
    return aiohttp.web.json_response(await servo.db.get(req),
                                     status=201)


@servo.auth.authenticate
async def put(req):
    if not req.has_body:
        log.error('{%s} PUT has no body' % req['context']['token']['id'])
        raise aiohttp.web.HTTPBadRequest()
    await servo.db.put(req)
    return aiohttp.web.json_response(await servo.db.get(req))


@servo.auth.authenticate
async def delete(req):
    return aiohttp.web.json_response({'message': 'Completed Successfuly'})
