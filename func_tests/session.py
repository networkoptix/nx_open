import os.path
import logging
import subprocess
from host import host_from_config


log = logging.getLogger(__name__)


class TestSession(object):

    def __init__(self, must_recreate_boxes):
        self._must_recreate_boxes = must_recreate_boxes
        self._boxes_recreated = False
        self.vagrant_private_key_fpath = None

    # done once per run, before all tests
    def init(self, run_options):
        if run_options.vm_ssh_host_config:
            self.vagrant_private_key_fpath = os.path.join(run_options.work_dir, 'vagrant_insecure_private_key')
            host = host_from_config(run_options.vm_ssh_host_config)
            log.debug('picking vagrant insecure ssh key:')
            host.get_file('.vagrant.d/insecure_private_key', self.vagrant_private_key_fpath)

    # boxes must be recreated only on first test setup
    def must_recreate_boxes(self):
        return self._must_recreate_boxes and not self._boxes_recreated

    def boxes_recreated(self):
        self._boxes_recreated = True


