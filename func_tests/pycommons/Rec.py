# $Id$
# Artem V. Nikitin
# Record - string-key dictionary replacer

class Rec:

    def __init__(self, init = {}, **kw):
        if isinstance(init, Rec): init = init.get()
        for name, val in init.items() + kw.items():
            setattr(self, name, val)

    def get(self, name = None):
        if name is None:
            return self.__dict__
        else:
            return getattr(self, name)

    def set(self, name, val):
        setattr(self, name, val)

    def asString(self):
        s = ', '.join(
          '%s=%s' % (str(key), str(val))
          for key, val in self.get().iteritems()
        )
        return '<%s>' % s

    def __repr__(self):
        return self.asString()

    def __str__(self):
        return self.asString()

    def __cmp__(self, other):
        if isinstance(other, Rec):
            return cmp(self.get(), other.get())
        return cmp(self.get(), other)
