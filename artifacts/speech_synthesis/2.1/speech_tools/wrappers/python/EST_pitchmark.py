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
            fp, pathname, description = imp.find_module('_EST_pitchmark', [dirname(__file__)])
        except ImportError:
            import _EST_pitchmark
            return _EST_pitchmark
        if fp is not None:
            try:
                _mod = imp.load_module('_EST_pitchmark', fp, pathname, description)
            finally:
                fp.close()
            return _mod
    _EST_pitchmark = swig_import_helper()
    del swig_import_helper
else:
    import _EST_pitchmark
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
import EST_Track
import EST_FVector

def neg_zero_cross_pick(*args):
  return _EST_pitchmark.neg_zero_cross_pick(*args)
neg_zero_cross_pick = _EST_pitchmark.neg_zero_cross_pick

def pm_fill(*args):
  return _EST_pitchmark.pm_fill(*args)
pm_fill = _EST_pitchmark.pm_fill

def pm_min_check(*args):
  return _EST_pitchmark.pm_min_check(*args)
pm_min_check = _EST_pitchmark.pm_min_check


def pitchmark(*args):
  return _EST_pitchmark.pitchmark(*args)
pitchmark = _EST_pitchmark.pitchmark

def pm_to_f0(*args):
  return _EST_pitchmark.pm_to_f0(*args)
pm_to_f0 = _EST_pitchmark.pm_to_f0

