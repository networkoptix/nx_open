from compinfo import fill_compatibility_matrix, version_to_string, COMPATIBILITY_MATRIX

def gencomp_cpp(xfile):
    header = """#include "compatibility.h"

QnCompatibilityInfo compatibilityInfo[] = {
"""

    footer = """
}"""

    fill_compatibility_matrix()

    print >> xfile, header
    sorted_matrix = list(COMPATIBILITY_MATRIX)
    sorted_matrix.sort(key=lambda x: x.v1[1] * 100 + hash(x.comp1) + x.v2[1])

    for ci in sorted_matrix:
        print >> xfile, '    { "%s", "%s", "%s" },' % (version_to_string(ci.v1), ci.comp1, version_to_string(ci.v2))
    print >> xfile, footer

def gencomp_py(xfile):
    header = """compatibility_info = (
"""

    footer = """
)"""

    fill_compatibility_matrix()

    print >> xfile, header
    sorted_matrix = list(COMPATIBILITY_MATRIX)
    sorted_matrix.sort(key=lambda x: x.v1[1] * 100 + hash(x.comp1) + x.v2[1])

    for ci in sorted_matrix:
        print >> xfile, '    ( "%s", "%s", "%s" ),' % (version_to_string(ci.v1), ci.comp1, version_to_string(ci.v2))
    print >> xfile, footer

if __name__ == '__main__':
    import sys
    gencomp_py(sys.stdout)
#    gencomp_cpp(open('build/compatibility.cpp'))
