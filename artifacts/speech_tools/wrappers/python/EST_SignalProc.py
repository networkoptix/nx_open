# This file was automatically generated by SWIG (http://www.swig.org).
# Version 1.3.40
#
# Do not make changes to this file unless you know what you are doing--modify
# the SWIG interface file instead.
# This file is compatible with both classic and new-style classes.

from sys import version_info
if version_info >= (2,6,0):
    def swig_import_helper():
        from os.path import dirname
        import imp
        fp = None
        try:
            fp, pathname, description = imp.find_module('_EST_SignalProc', [dirname(__file__)])
        except ImportError:
            import _EST_SignalProc
            return _EST_SignalProc
        if fp is not None:
            try:
                _mod = imp.load_module('_EST_SignalProc', fp, pathname, description)
            finally:
                fp.close()
            return _mod
    _EST_SignalProc = swig_import_helper()
    del swig_import_helper
else:
    import _EST_SignalProc
del version_info
try:
    _swig_property = property
except NameError:
    pass # Python < 2.2 doesn't have 'property'.
def _swig_setattr_nondynamic(self,class_type,name,value,static=1):
    if (name == "thisown"): return self.this.own(value)
    if (name == "this"):
        if type(value).__name__ == 'SwigPyObject':
            self.__dict__[name] = value
            return
    method = class_type.__swig_setmethods__.get(name,None)
    if method: return method(self,value)
    if (not static) or hasattr(self,name):
        self.__dict__[name] = value
    else:
        raise AttributeError("You cannot add attributes to %s" % self)

def _swig_setattr(self,class_type,name,value):
    return _swig_setattr_nondynamic(self,class_type,name,value,0)

def _swig_getattr(self,class_type,name):
    if (name == "thisown"): return self.this.own()
    method = class_type.__swig_getmethods__.get(name,None)
    if method: return method(self)
    raise AttributeError(name)

def _swig_repr(self):
    try: strthis = "proxy of " + self.this.__repr__()
    except: strthis = ""
    return "<%s.%s; %s >" % (self.__class__.__module__, self.__class__.__name__, strthis,)

try:
    _object = object
    _newclass = 1
except AttributeError:
    class _object : pass
    _newclass = 0


import EST_Wave

def FIR_double_filter(*args):
  return _EST_SignalProc.FIR_double_filter(*args)
FIR_double_filter = _EST_SignalProc.FIR_double_filter

def lpc_filter(*args):
  return _EST_SignalProc.lpc_filter(*args)
lpc_filter = _EST_SignalProc.lpc_filter

def inv_lpc_filter(*args):
  return _EST_SignalProc.inv_lpc_filter(*args)
inv_lpc_filter = _EST_SignalProc.inv_lpc_filter

def lpc_filter_1(*args):
  return _EST_SignalProc.lpc_filter_1(*args)
lpc_filter_1 = _EST_SignalProc.lpc_filter_1

def lpc_filter_fast(*args):
  return _EST_SignalProc.lpc_filter_fast(*args)
lpc_filter_fast = _EST_SignalProc.lpc_filter_fast

def inv_lpc_filter_ola(*args):
  return _EST_SignalProc.inv_lpc_filter_ola(*args)
inv_lpc_filter_ola = _EST_SignalProc.inv_lpc_filter_ola

def simple_mean_smooth(*args):
  return _EST_SignalProc.simple_mean_smooth(*args)
simple_mean_smooth = _EST_SignalProc.simple_mean_smooth

def design_FIR_filter(*args):
  return _EST_SignalProc.design_FIR_filter(*args)
design_FIR_filter = _EST_SignalProc.design_FIR_filter

def design_lowpass_FIR_filter(*args):
  return _EST_SignalProc.design_lowpass_FIR_filter(*args)
design_lowpass_FIR_filter = _EST_SignalProc.design_lowpass_FIR_filter

def design_highpass_FIR_filter(*args):
  return _EST_SignalProc.design_highpass_FIR_filter(*args)
design_highpass_FIR_filter = _EST_SignalProc.design_highpass_FIR_filter


def FIRfilter(*args):
  return _EST_SignalProc.FIRfilter(*args)
FIRfilter = _EST_SignalProc.FIRfilter

def FIRlowpass_filter(*args):
  return _EST_SignalProc.FIRlowpass_filter(*args)
FIRlowpass_filter = _EST_SignalProc.FIRlowpass_filter

def FIRhighpass_filter(*args):
  return _EST_SignalProc.FIRhighpass_filter(*args)
FIRhighpass_filter = _EST_SignalProc.FIRhighpass_filter

def FIRhighpass_double_filter(*args):
  return _EST_SignalProc.FIRhighpass_double_filter(*args)
FIRhighpass_double_filter = _EST_SignalProc.FIRhighpass_double_filter

def FIRlowpass_double_filter(*args):
  return _EST_SignalProc.FIRlowpass_double_filter(*args)
FIRlowpass_double_filter = _EST_SignalProc.FIRlowpass_double_filter

def pre_emphasis(*args):
  return _EST_SignalProc.pre_emphasis(*args)
pre_emphasis = _EST_SignalProc.pre_emphasis

def post_emphasis(*args):
  return _EST_SignalProc.post_emphasis(*args)
post_emphasis = _EST_SignalProc.post_emphasis

