import string

from compatibility import *

CL = Component('Client', 'HD Witness Client')
MS = Component('MediaServer', 'HD Witness MediaServer')
IOSCL = Component('iOSClient', 'iOS HD Witness Client')
ANDROID = Component('android', 'Android HD Witness Client')

V10 = Version(1, 0)
V11 = Version(1, 1)
V12 = Version(1, 2)
V13 = Version(1, 3)
V14 = Version(1, 4)
V15 = Version(1, 5)
V16 = Version(1, 6)
V20 = Version(2, 0)
V21 = Version(2, 1)
           
COMPATIBILITY_INFO = (
    (V15, (IOSCL,), V16), # iOS V1.5 can connect to 1.6
    (V15, (IOSCL,), V20), # iOS V1.5 can connect to 2.0

    (V20, (IOSCL,), V15), # iOS V2.0 can connect to 1.5
    (V20, (IOSCL,), V16), # iOS V2.0 can connect to 1.6

    (V16, (ANDROID,), V14), # android V1.6 can connect to 1.4
    (V16, (ANDROID,), V15), # android V1.6 can connect to 1.5
    (V16, (ANDROID,), V20), # android V1.6 can connect to 2.0
    
    (V16, (CL,), V20), # client V1.6 can connect to 2.0
    (V20, (CL,), V16), # client V2.0 can connect to 1.6
    
    (V16, (MS,), V20), # MediaServer V1.6 can connect to 2.0
    (V20, (MS,), V16), # MediaServer V2.0 can connect to 1.6

    (V20, (CL,MS,IOSCL,ANDROID), V21),
    (V21, (CL,MS,IOSCL,ANDROID), V20),

    (V16, (CL,MS,IOSCL,ANDROID), V21),
    (V21, (CL,MS,IOSCL,ANDROID), V16),

#    (V13, (CL,), Range(V12, V12)),
#    (V14, (CL,), Range(V12, V13)),
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
    print is_compatible('Client', '1.6', 'Server', '2.0')
    print is_compatible('Client', '2.0', 'Server', '1.6')

    
