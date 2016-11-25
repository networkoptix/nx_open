# $Id$
# Artem V. Nikitin
# logger class

from Utils import *
import time, sys, string, traceback, StringIO, thread, os, codecs, types
from threading import Lock, currentThread

if sys.platform != 'win32':
  import syslog
  

logEncoding = 'koi8-r'
defLogLevel = 5
normalLevel = 20  # level num for massive messages must be greater than this
maxLFileSize = 100 * 1024  # max and min log file size for windows for AutoTruncateLogger
minLFileSize =  10 * 1024


def makePrefix( level, useTime ):
  if useTime:
    t = time.time()
    ms = int((t - int(t)) * 1000) % 1000
    timeStr = '%s.%03d ' % (time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(t)), ms)
  else:
    timeStr = ''
  threadName = currentThread().getName()
  return '%s[%s] %02d' % (timeStr, threadName, level)
  #return '%s[%s|%d-%d] %02d' % (timeStr, threadName, os.getpid(), thread.get_ident(), level)
  #return '%s[%d][%s] %02d' % (timeStr, os.getpid(), threadName, level)


class Logger:

  def __init__( self, logLevel ):
    self.__logLevel__ = logLevel
    self.__mutex__ = Lock()
    self.__encoder__ = codecs.getencoder(logEncoding)
  
  def logLogLevel( self ):
    self.log(0, 'Log level set to %d' % self.logLevel())

  def logLevel( self ):
    self.__mutex__.acquire()
    try:
      return self.__logLevel__
    finally:
      self.__mutex__.release()

  def setLogLevel( self, level ):
    self.__mutex__.acquire()
    self.__logLevel__ = level
    self.__mutex__.release()
    self.logLogLevel()

  def isSystem(self):
    return False

  def log( self, level, msg ):
    self.__mutex__.acquire()
    try:
      if level <= self.__logLevel__:
        if type(msg) is types.UnicodeType:
          msg, len = self.__encoder__(msg, 'backslashreplace')
        for line in msg.split('\n'):
          self._log(level, line)
    finally:
      self.__mutex__.release()

  # to be called from SIGHUP handler
  def reopen( self ):
    self.__mutex__.acquire()
    try:
      self._reopen()
    finally:
      self.__mutex__.release()

  def _reopen( self ):
    pass
    

class FileLogger(Logger):
  
  def __init__( self, logLevel, fileName = None, alwaysSync = False, rewrite = False ):
    Logger.__init__(self, logLevel)
    self.__fileName__ = fileName
    self.__isSystem__ = True
    if fileName is None or fileName == '-':
      self.__file__ = sys.stdout
    elif fileName == 'stderr':
      self.__file__ = sys.stderr
    else:
      self.__isSystem__ = False
      self.__file__ = file(fileName, rewrite and 'w' or 'a+')
    self.alwaysSync = alwaysSync

  def isSystem(self):
    return self.__file__ in [sys.stdout, sys.stderr]

  def _log( self, level, msg ):
    print >> self.__file__, '%s  %s' % (makePrefix(level, True), msg)
    if level <= normalLevel or self.alwaysSync: self.__file__.flush()

  def _reopen( self ):
    if self.__fileName__ is None or self.__fileName__ == '-': return
    self._log(0, 'Reopening...')
    self.__file__.close()
    self.__file__ = file(self.__fileName__, 'a+')
    self._log(0, 'Reopened. Log level = %d' % self.__logLevel__)
    

class SysLogger(Logger):

  def __init__( self, app, logLevel ):
    Logger.__init__(self, logLevel)
    syslog.openlog(app, syslog.LOG_PID, syslog.LOG_USER)

  def isSystem(self):
    return True

  def _getPriority( self, level ):
    if level <=  2: return syslog.LOG_ERR
    if level <=  3: return syslog.LOG_WARNING
    if level <=  5: return syslog.LOG_NOTICE
    if level <= 10: return syslog.LOG_INFO
    return syslog.LOG_DEBUG

  def _log( self, level, msg ):
    syslog.syslog(self._getPriority(level), '%s  %s' % (makePrefix(level, False), msg))


# drops oldest lines from log file periodically, keeping maximum and minimum log sizes
class AutoTruncateLogger(FileLogger):

  def __init__( self, logLevel, fileName, alwaysSync, maxSize, minSize ):
    FileLogger.__init__(self, logLevel, fileName, alwaysSync)
    self.__maxSize__ = maxSize
    self.__minSize__ = minSize

  def _log( self, level, msg ):
    FileLogger._log(self, level, msg)
    self._checkSize()

  def _checkSize( self ):
    f = self.__file__
    if f.tell() < self.__maxSize__: return
    f.seek(-self.__minSize__, 2)  # 2 means relative to the file's end
    lines = f.readlines()
    f.seek(0)
    f.truncate()
    f.writelines(lines[1:])  # first line is not complete
    FileLogger._log(self, 0, 'Log is truncated to approx %d bytes / %d lines.' % (self.__minSize__, len(lines)))


class NullLogger(Logger):

  def _log( self, level, msg ):
    pass


logger = FileLogger(defLogLevel)


def _initLog( logLevel = None, fileName = None, alwaysSync = 0, app = 'python', rewrite = False ):
  #use logger.set please
  global logger
  if logLevel is None: logLevel = defLogLevel
  if sys.platform != 'win32' and fileName == 'syslog':
    logger = SysLogger(app, logLevel)
  elif fileName == 'none':
    logger = NullLogger(0)
  elif sys.platform == 'win32':
    logger = AutoTruncateLogger(logLevel, fileName, alwaysSync, maxLFileSize, minLFileSize)
  else:
    logger = FileLogger(logLevel, fileName, alwaysSync, rewrite)
  logger.logLogLevel()

def initLog( logLevel = None, fileName = None, app = 'python', rewrite = False ):
  alwaysSync = 0
  for v in sys.argv:
    l = string.split(v, '=')
    if len(l) == 2 and l[0] == '--loglevel':
      logLevel = int(l[1])
    if len(l) == 2 and l[0] == '--log':
      fileName = l[1]
    if len(l) == 2 and l[0] == '--logsync':
      alwaysSync = int(l[1])
  _initLog(logLevel, fileName, alwaysSync, app, rewrite)


def logLevel():
  return logger.logLevel()

def setLogLevel( level ):
  logger.setLogLevel(level)
  
def log( level, msg ):
  if logger:
    logger.log(level, msg)
  else:
    print '%s [starting]: %s' % (makePrefix(level, True), msg)

def logReopen():
  if logger:
    logger.reopen()


def logException():
  for line in traceback.format_exc().splitlines():
    log(1, '  %s' % line)
  # unit test support
  # log Exception executed on failed thread - THIS trace we want to see on unittest output
  setThreadTrace()

def strException():
  f = StringIO.StringIO()
  traceback.print_exc(100, f)
  trace = f.getvalue()
  return trace

# unit test support
_threadExInfo = None

def setThreadTrace():
  global _threadExInfo
  if not _threadExInfo: # or we are not first failed thread
    _threadExInfo = sys.exc_info()

def clearThreadTrace():
  global _threadExInfo
  _threadExInfo = None

def getThreadTrace():
  return _threadExInfo
