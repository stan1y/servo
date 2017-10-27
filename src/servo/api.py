import logging
import aiohttp
import aiohttp.web

import servo.auth
import servo.db


log = logging.getLogger(__name__)


async def response(req, status, sbody):
    return aiohttp.web.Response(
        status=status,
        content_type=req['context']['out_type'],
        body=sbody.encode(req.charset))


@servo.auth.authenticate
async def get(req):
    return aiohttp.web.Response(
        content_type=req['context']['out_type'],
        body=await servo.db.get(req))


@servo.auth.authenticate
async def post(req):
    if not req.has_body:
        log.error('{%s} POST has no body' % req['context']['token']['id'])
        raise aiohttp.web.HTTPBadRequest()
    await servo.db.post(req)
    return aiohttp.web.Response(
        content_type=req['context']['out_type'],
        body=await servo.db.get(req),
        status=201)


@servo.auth.authenticate
async def put(req):
    if not req.has_body:
        log.error('{%s} PUT has no body' % req['context']['token']['id'])
        raise aiohttp.web.HTTPBadRequest()
    await servo.db.put(req)
    return aiohttp.web.Response(
        content_type=req['context']['out_type'],
        body=await servo.db.get(req))


@servo.auth.authenticate
async def delete(req):
    return aiohttp.web.json_response({'message': 'Completed Successfuly'})
