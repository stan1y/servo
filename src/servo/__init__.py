import os
import argparse
import aiohttp
import aiohttp.web
import configparser
import logging
import servo.web


_config_paths = ['$HOME/.servo/conf', '/etc/servo/conf']


log = logging.getLogger(__name__)


def merge_args(cfg, args):
    cfg['servo']['debug'] = 'yes' if args.debug else 'no'
    cfg['servo']['ssl'] = 'yes' if args.ssl else 'no'


def read_config(args):
    for p in _config_paths:
        path = os.path.expandvars(p)
        if os.path.exists(path):
            log.debug('reading configuration %s' % path)
            with open(path, 'r') as f:
                cfg = configparser.ConfigParser()
                cfg.read_file(f)
                merge_args(cfg, args)
                return cfg
    raise Exception('No configuration file found at paths: %s' %
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
    parser.add_argument('--debug', default=False, action="store_true",
                        help='Enable debug mode')
    parser.add_argument('--ssl', default=False, action="store_true",
                        help='Enable SSL endpoint')

    args = parser.parse_args()
    configure_log(args)

    cfg = read_config(args)
    servo_app = servo.web.create_app(cfg)

    aiohttp.web.run_app(
        app=servo_app,
        host=cfg['servo']['listen'],
        port=int(cfg['servo']['port']),
        ssl_context=servo.web.create_ssl_context(cfg),
        access_log=None,
    )
