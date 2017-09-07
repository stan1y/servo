import os
import argparse
import asyncio
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
    raise Exception('No configuration file found at paths: %s' % ','.join(_config_paths))


def configure_log(args):
    lvl=logging.INFO
    if args.debug:
        lvl=logging.DEBUG
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

    # parser.add_argument('-u', '--username', default=USERNAME,
    #                     help='Google Apps username ($GOOGLE_USERNAME)')
    # parser.add_argument('-I', '--idp-id', default=IDP_ID,
    #                     help='Google SSO IDP identifier ($GOOGLE_IDP_ID)')
    # parser.add_argument('-S', '--sp-id', default=SP_ID,
    #                     help='Google SSO SP identifier ($GOOGLE_SP_ID)')
    # parser.add_argument('-R', '--region', default=REGION,
    #                     help='AWS region endpoint ($AWS_DEFAULT_REGION)')
    # parser.add_argument('-d', '--duration', type=int, default=DURATION,
    #                     help='Credential duration ($DURATION)')
    # parser.add_argument('-p', '--profile', default=PROFILE,
    #                     help='AWS profile ($AWS_PROFILE)')
    # parser.add_argument('-V', '--version', action='version',
    #                     version='%(prog)s {version}'.format(version=_version.__version__))

    args = parser.parse_args()
    configure_log(args)

    cfg = read_config(args)
    app = servo.web.create_app(cfg)

    aiohttp.web.run_app(app,
        host=cfg['servo']['listen'],
        port=int(cfg['servo']['port']),
        ssl_context=servo.web.create_ssl_context(cfg),
        access_log=logging.getLogger('servo.access') if args.debug else None,
    )
