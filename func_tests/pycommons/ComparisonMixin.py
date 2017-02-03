# $Id$
# Artem V. Nikitin
# Comparison mixin for testing

from Rec import Rec
from Utils import struct2str
import types, operator

class ComparisonMixin(object):

    class XInvAttr(Exception): pass


    def __v2str(self, v):
        return struct2str(v, deep=True)

    def compare(self, expect, got, desc = 'item'):
        if type(expect) is dict:
            expect = Rec(expect)
        if type(got) is dict:
            got = Rec(got)
        if type(expect) is types.InstanceType:
            self.compareRec(expect, got, desc)
        elif type(expect) is type(()):
            self.compareTuple(expect, got, desc)
        elif type(expect) is type([]):
            self.compareList(expect, got, desc)
        else:
            self.compareVal(expect, got, desc)

    def compareVal(self, expect, got, desc):
        if got != expect:
            self.fail('%s mismatch: expected %s, but got %s' %
                      (desc, self.__v2str(expect), self.__v2str(got)))

    def compareRec(self, expect, got, desc = 'item'):
        if expect is None:
            if got is None: return
            self.fail('%s mismatch: expected None, but got %s' % (desc, self.__v2str(got)))
        expectCls = None
        if isinstance(expect, Rec):
            if hasattr(expect, '_class'): expectCls = expect._class
        else:
            expectCls = expect.__class__
        if expectCls is not None and expectCls != got.__class__:
            self.fail('%s: expected %s, but got %s' % (desc, expect.__class__, got.__class__))
        for name, expectVal in expect.__dict__.items():
            if name == '_class': continue
            if not hasattr(got, name):
                raise self.XInvAttr('%s: unknown attribute "%s" (spelling error?)' % (desc, name))
            gotVal = getattr(got, name)
            self.compare(expectVal, gotVal, '%s.%s' % (desc, name))

    def compareList(self, expect, got, desc):
        self.assert_(type(got) is list, 'expected %s, list, but got %s' % (desc, got))
        for i in xrange(min(len(expect), len(got))):
            self.compare(expect[i], got[i], '%s#%d' % (desc, i))
        if len(expect) < len(got):
            self.fail('got extra %s#%d (first: %s)' % (desc, len(expect), got[len(expect)]))
        if len(got) < len(expect):
            self.fail('%s#%d absent' % (desc, len(got)))

    def compareTuple(self, expect, got, desc):
        if (expect is None) and (got is None): return True
        if (expect is None) or (got is None):
            self.fail('%s tuple mismatch (expected %s, got %s)' % (desc, expect, got))
        if len(expect) <> len(got):
            self.fail('%s tuple length mismatch (expected %d, got %d)' % (desc, len(expect), len(got)))
        i = 0
        for expectVal, gotVal in zip(expect, got):
            self.compare(expectVal, gotVal, '%s#%d' % (desc, i))
            i += 1

    # check if list include given item
    def assertHasItem(self, item, gotList, desc = 'item'):
        for i, r in enumerate(gotList):
            try:
                self.compare(item, r, desc)
                return i  # this is our item - test passed
            except self.failureException:
                pass  # try another
            except self.XInvAttr:
                pass  # try another
        self.fail('No %s (%s) found' % (desc, self.__v2str(item)))

    # check if list does not include given item
    def assertHasNotItem(self, item, gotList, desc = 'item'):
        found = False
        for i, r in enumerate(gotList):
            try:
                self.compare(item, r, desc)
                found = True
                break
            except self.failureException:
                pass
            except self.XInvAttr:
                pass  # try another
        if found:
            self.fail('%s (%s) found' % (desc, self.__v2str(item)))

    def assertHasItems(self, items, gotList, desc = 'item'):
        for item in items:
            self.assertHasItem(item, gotList, desc)

    def compareUnorderedList(self, expect, got, desc = 'item'):
        found = set()
        for i, item in enumerate(expect):
            idx = self.assertHasItem(item, got, '%s#%d' % (desc, i))
            found.add(idx)
        for i, item in enumerate(got):
            if i not in found:
                self.fail('Got extra %s#%d: %s' % (desc, i, self.__v2str(item)))

    compareUnorderedRecList = compareUnorderedList  # for backward compatibility, temporary

    def assertHasOrderedItems(self, expect, got, desc = 'item'):
        prIdx  = 0
        idx    = 0
        prItem = None
        for i, item in enumerate(expect):
            idx = self.assertHasItem(item, got, '%s#%d' % (desc, i))
            if idx < prIdx:
                self.fail('Sequence order violation %s: got %s after %s' % \
                          (desc, self.__v2str(prItem), self.__v2str(item)))
            prIdx = idx
            prItem = item

    def assertIsSorted(self, seq, desc = 'items', op = operator.le):
        if not all(op(seq[i], seq[i+1]) for i in xrange(len(seq)-1)):
            self.fail("%s' sequence is not sorted" % desc)
        

    def isEqual(self, expect, got):
        try:
            self.compare(expect, got)
            return True
        except self.failureException:
            pass
        except self.XInvAttr:
            pass
        return False
