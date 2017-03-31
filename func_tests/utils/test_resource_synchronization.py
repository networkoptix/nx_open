#!/usr/bin/env python

import sys
sys.path.append('..')

import logging
import traceback
import resource_synchronization_test as test
from optparse import OptionParser
from ConfigParser import ConfigParser
from test_utils.server_rest_api import ServerRestApi
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


class RemoteServer(object):

    def __init__(self, name, box, user, password):
        self.title = name
        self.box = box
        self.url = 'http://%s/' % self.box
        self.user = user
        self.password = password
        self.ecs_guid = None
        self.settings = None
        self.local_system_id = None
        self.rest_api = ServerRestApi(self.title, self.url, self.user, self.password)
        self.load_system_settings()

    def load_system_settings(self):
        self.settings = self.get_system_settings()
        self.local_system_id = self.settings['localSystemId']
        self.cloud_system_id = self.settings['cloudSystemID']
        self.ecs_guid = self.rest_api.ec2.testConnection.GET()['ecsGuid']

    def get_system_settings(self):
        response = self.rest_api.api.systemSettings.GET()
        return response['settings']


def read_servers(config):
    server_list = config.get(DEFAULT_CONFIG_SECTION, 'serverList')
    return server_list.split(",")


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
    usage = "usage: %prog [options]"
    parser = OptionParser(usage)
    parser.add_option("--config-file-path",
                      help="Test configuration file.")
    parser.add_option('--max-log-width', default=DEFAULT_MAX_LOG_WIDTH, type=int,
                      help='Change maximum log message width. Default is %d' % DEFAULT_MAX_LOG_WIDTH)
    parser.add_option('--verbosity', '-v', action='count')
    (options, args) = parser.parse_args()

    if not options.config_file_path:
        print >> sys.stderr, "%s: --config-file-path parameter required" % sys.argv[0]
        parser.print_help()
        exit(1)

    config = ConfigParser()
    config.read(options.config_file_path)

    log_level = {
        1: logging.ERROR,
        2: logging.WARNING,
        3: logging.INFO,
        4: logging.DEBUG}.get(options.verbosity, logging.ERROR)

    log_format = '%%(asctime)-15s %%(threadName)-15s %%(levelname)-7s %%(message).%ds' % 1000
    logging.basicConfig(level=log_level, format=log_format)
    servers = {'srv_%d' % idx:
               RemoteServer('srv-%d' % idx,
                            srv,
                            config.get(DEFAULT_CONFIG_SECTION, CONFIG_USERNAME),
                            config.get(DEFAULT_CONFIG_SECTION, CONFIG_PASSWORD))
               for idx, srv in enumerate(read_servers(config))}
    env = SimpleNamespace(servers=servers,
                          test_size=config.getint(DEFAULT_CONFIG_SECTION, CONFIG_TESTSIZE),
                          thread_number=config.getint(DEFAULT_CONFIG_SECTION, CONFIG_THREADS),
                          system_merged=True,
                          resource_generators=test.resource_generators())
    tests = ['test_initial_merge',
             'test_api_get_methods',
             'test_camera_data_syncronization',
             'test_user_data_syncronization',
             'test_mediaserver_data_syncronization',
             'test_storage_data_synchronization',
             'test_layout_data_syncronization',
             'test_resource_params_data_syncronization',
             'test_remove_resource_params_data_syncronization',
             'test_resource_remove_update_conflict',]
    for test_name in tests:
        run_test(test_name, env)

main()
