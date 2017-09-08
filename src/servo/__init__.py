import os
import sys
import daemon
import argparse
import aiohttp
import aiohttp.web
import configparser
import logging

import servo.web
import servo.err

log = logging.getLogger(__name__)
_config_paths = ['$HOME/.servo/conf', '/etc/servo/conf']


# Servo Item Types
_servo_types = [
    'text/plain',
    'text/html',
    'application/json',
    'application/base64',
    'multipart/form-data'
]


def get_content_type(ctype):
    for t in _servo_types:
        if t in ctype:
            return t
    return None


def read_out_type(req):
    '''Read accept headers and convert to servo item type'''
    accepts = req.headers.getall(aiohttp.hdrs.ACCEPT, ['text/plain'])
    for ctype in accepts:
        stype = get_content_type(ctype)
        if stype:
            return stype
    log.error('unsupported accepted types: %s' % (
        ', '.join(accepts)))
    raise aiohttp.web.HTTPBadRequest()


def read_in_type(req):
    '''Read content-type headers and convert to servo item type'''
    types = req.headers.getall(aiohttp.hdrs.CONTENT_TYPE, ['text/plain'])
    for ctype in types:
        stype = get_content_type(ctype)
        if stype:
            return stype
    log.error('unsupported content types: %s' % (
        ', '.join(types)))
    raise aiohttp.web.HTTPBadRequest()


def read_config(path):
    '''Read configuration file from path'''
    log.debug('reading configuration %s' % path)
    with open(path, 'r') as f:
        cfg = configparser.ConfigParser()
        cfg.read_file(f)
        return cfg
    raise servo.err.ConfigurationError(
        'Failed to read configuration from %s' % path)


def load_config(args):
    '''Load configuration from possible locations'''
    if args.config and os.path.exists(args.config):
        return read_config(args.config)

    for p in _config_paths:
        path = os.path.expandvars(p)
        if os.path.exists(path):
            return read_config(path)

    raise servo.err.ConfigurationError(
        'No configuration file found at paths: %s' %
        ','.join(_config_paths))


def configure_log(args):
    lvl = logging.INFO
    if args.debug:
        lvl = logging.DEBUG
    logging.basicConfig(level=lvl)


def main():
    parser = argparse.ArgumentParser(
        prog='servo',
        description='Servo engine command line interface',
    )
    parser.add_argument('-D', '--daemon', default=False, action='store_true',
                        help='Execute servo in daemon context and '
                             'detach command line process.')
    parser.add_argument('--debug', default=False, action='store_true',
                        help='Enable debug mode')
    parser.add_argument('--config',
                        help='Path to configuration file to use, otherwise '
                             'use ~/.servo/conf or /etc/servo/conf.')

    args = parser.parse_args()
    configure_log(args)

    if not args.daemon:
        servo_entry(args)
        sys.exit(0)

    with daemon.DaemonContext():
        servo_entry(args)
    print('detached daemon')
    sys.exit(0)


def servo_entry(args):
    '''Create and run Servo application'''
    try:
        cfg = load_config(args)
        servo_app = servo.web.create_app(cfg)

        aiohttp.web.run_app(
            app=servo_app,
            host=cfg['servo']['listen'],
            port=int(cfg['servo']['port']),
            ssl_context=servo.web.create_ssl_context(cfg),
            access_log=None,
        )
        sys.exit(0)

    except servo.err.ConfigurationError as cfgerr:
        log.error('failed to start: %s' % cfgerr)
        sys.exit(1)
