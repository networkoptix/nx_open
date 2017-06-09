#!/usr/bin/env python

import sys
sys.path.append('..')

import uuid
import logging
import traceback
import argparse
import urllib
import base64
import time
import resource_synchronization_test as test
from ConfigParser import ConfigParser
from test_utils.server import Server
from test_utils.utils import SimpleNamespace

DEFAULT_CONFIG_SECTION = 'General'
DEFAULT_TEST_SIZE = 10
DEFAULT_THREAD_NUMBER = 8
CONFIG_USERNAME = 'username'
CONFIG_PASSWORD = 'password'
CONFIG_TESTSIZE = 'testCaseSize'
CONFIG_THREADS = 'threadNumber'
DEFAULT_MAX_LOG_WIDTH = 1000

log = logging.getLogger(__name__)


def read_servers(config):
    server_list = config.get(DEFAULT_CONFIG_SECTION, 'serverList')

    def endpoint_from_str(s):
        endpoint_list = s.split(':') + [None]
        return tuple(endpoint_list[0:2])

    return map(endpoint_from_str, server_list.split(","))


def run_test(test_name, env):
    sys.stdout.write('%s ... ' % test_name)
    sys.stdout.flush()
    try:
        test_fn = getattr(test, test_name)
        test_fn(env)
        sys.stdout.write('OK\n')
    except Exception:
        sys.stdout.write("FAIL\n\n'%s'\n\n" % traceback.format_exc())
    sys.stdout.flush()


def main():
    parser = argparse.ArgumentParser(usage='%(prog)s [options]')
    parser.add_argument("config_file_path", nargs=1, help="Test configuration file.")
    parser.add_argument('--max-log-width', default=DEFAULT_MAX_LOG_WIDTH, type=int,
                        help='Change maximum log message width. Default is %d' % DEFAULT_MAX_LOG_WIDTH)
    parser.add_argument('--verbosity', '-v', action='count')
    args = parser.parse_args()

    config = ConfigParser()
    config.read(args.config_file_path)

    log_level = {
        1: logging.ERROR,
        2: logging.WARNING,
        3: logging.INFO,
        4: logging.DEBUG}.get(args.verbosity, logging.ERROR)

    log_format = '%%(asctime)-15s %%(threadName)-15s %%(levelname)-7s %%(message).%ds' % 1000
    logging.basicConfig(level=log_level, format=log_format)
    username = config.get(DEFAULT_CONFIG_SECTION, CONFIG_USERNAME)
    password = config.get(DEFAULT_CONFIG_SECTION, CONFIG_PASSWORD)
    test_size = config.getint(DEFAULT_CONFIG_SECTION, CONFIG_TESTSIZE)
    thread_number = config.getint(DEFAULT_CONFIG_SECTION, CONFIG_THREADS)
    servers = dict()
    for idx, (host, port) in enumerate(read_servers(config)):
        server_name = 'Server_%d' % idx
        rest_api_url = 'http://%s:%s/' % (host, port)
        server = Server('networkoptix', server_name, rest_api_url, internal_ip_port=port)
        server._is_started = True
        server.load_system_settings()
        servers[server_name] = server
    env = SimpleNamespace(servers=servers,
                          test_size=test_size,
                          thread_number=thread_number,
                          system_is_merged=True,
                          resource_generators=test.resource_generators())
    for name in dir(test):
        if name.startswith('test_'):
            run_test(name, env)

if __name__ == "__main__":
    main()
