PLUGIN_TYPE = video/videonode
PLUGIN_EXTENDS = quick
PLUGIN_CLASS_NAME = QnBpiSGVideoNodeFactoryPlugin
load(qt_plugin)

DESTDIR = $$OUTPUT_PATH/bin/$$CONFIGURATION/$$PLUGIN_TYPE

CONFIG -= precompile_header
