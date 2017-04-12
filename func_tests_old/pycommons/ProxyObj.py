# $Id$
# base class for proxy objects - relays all method calls to somethere

import Utils

class ProxyObj(object):

    class ProxyMethod(object):

        def __init__( self, obj, method ):
            self.__obj    = obj
            self.__method = method

        def __call__( self, *args, **kw ):
            return self.__obj._proxyObjCall(self.__method, *args, **kw)

    def __getattr__( self, name ):
        return self.ProxyMethod(self, name)

    @Utils.abstract
    def _proxyObjCall( self, methodName, *args, **kw ): pass
