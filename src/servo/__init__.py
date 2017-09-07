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


_config_paths = ['$HOME/.servo/conf', '/etc/servo/conf']

# Servo Item Types
TYPE_STRING = 1
TYPE_JSON = 2
TYPE_BLOB = 3
TYPE_HTML = 4

log = logging.getLogger(__name__)


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
