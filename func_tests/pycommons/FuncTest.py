# $Id$
# Artem V. Nikitin
# base classes for functional tests

import subprocess
from Logger import log, makePrefix, LOGLEVEL

class XTimedOut(Exception): pass

VSSH_CMD = "./vssh-ip.sh"
MEDIA_SERVER_DIR='/opt/networkoptix/mediaserver'

# test logging
def tlog( level, msg ):
  log(level, 'TEST: %s' % msg)

def execVBoxCmd(host, *command):
  cmd = ' '.join(command)
  try:
    tlog(LOGLEVEL.DEBUG + 4, "DEBUG: %s" % str([VSSH_CMD, host, 'sudo'] + list(command))) 
    output = subprocess.check_output(
      [VSSH_CMD, host, 'sudo'] + list(command),
      shell=False, stderr=subprocess.STDOUT)
    tlog(LOGLEVEL.DEBUG + 4, "%s command '%s':\n%s" % (host, cmd, output))
  except subprocess.CalledProcessError, x:
    tlog(LOGLEVEL.DEBUG + 4, "%s command '%s' failed, code=%d:\n%s" % \
         (host, cmd, x.returncode, x.output))
    raise
