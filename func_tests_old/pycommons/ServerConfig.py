# $Id$
# Artem V. Nikitin
# Server config preparation

from ConfigParser import RawConfigParser

CfgFileName          = 'mediaserver.conf'
RuntimeCfgFileName   = 'mediaserver_runtime.conf'

DEFAULT_CONFIG = {
  'noSetupWizard': 1,
  'removeDbOnStartup': 1,
#  'systemIdFromSystemName': 1,
  'port': 7001,
  'appserverPassword': '',
  'rtspTransport': '',
  'logLevel': 'DEBUG2',
#  'systemName':'functesting',
  'cameraSettingsOptimization': 'true',
  'statisticsReportAllowed':'true' }

def set_config_section(
  config, section, defaults=DEFAULT_CONFIG, values={}):
    if not config.has_section(section):
        config.add_section(section)
    assert isinstance(DEFAULT_CONFIG, dict)
    assert isinstance(values, dict)
    cfg = defaults.copy()
    cfg.update(**values)
    for k, v in cfg.items():
        config.set(section, k, v)
