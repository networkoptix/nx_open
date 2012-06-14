import string

from compatibility import *

CL = Component('Client', 'HD Witness Client')

V10 = Version(1, 0)
V11 = Version(1, 1)
V12 = Version(1, 2)
V13 = Version(1, 3)
V14 = Version(1, 4)
           
COMPATIBILITY_INFO = (
    (V13, (CL,), Range(V12, V12)),
    (V14, (CL,), Range(V12, V13)),
)
COMPATIBILITY_MATRIX = set()

class CompatibilityItem(object):
    def __init__(self, v1, comp1, v2):
        self.v1 = v1
        self.comp1 = comp1
        self.v2 = v2

    def __hash__(self):
        return self.v1[1] + 7 * hash(string.join(self.comp1,".")) + 13 * self.v2[1]

    def __eq__(self, other):
        return self.v1 == other.v1 and \
            self.comp1 == other.comp1 and \
            self.v2 == other.v2


def fill_compatibility_matrix():
    for ver1list, clist, ver2list in COMPATIBILITY_INFO:
        for ver1 in ver1list:
            for comp in clist:
                for ver2 in ver2list:
                    COMPATIBILITY_MATRIX.add(CompatibilityItem(ver1, comp.name, ver2))

def is_compatible(comp1, v1s, comp2, v2s):
    "Is component comp1 of version v1s (as string) and comp2 of version v2s compatible?"
    v1 = tuple(map(int, v1s.split('.')[0:2]))
    v2 = tuple(map(int, v2s.split('.')[0:2]))

    return CompatibilityItem(v1, comp1, v2) in COMPATIBILITY_MATRIX \
        or CompatibilityItem(v2, comp2, v1) in COMPATIBILITY_MATRIX

def version_to_string(version):
    return string.join(map(str, version), '.')

if __name__ == '__main__':
    fill_compatibility_matrix()
    for ci in COMPATIBILITY_MATRIX:
        print version_to_string(ci.v1), ci.comp1, version_to_string(ci.v2)

    print is_compatible('Client', '1.3', 'Server', '1.2')

    