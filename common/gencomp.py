from compinfo import fill_compatibility_matrix, version_to_string, COMPATIBILITY_MATRIX

def gencomp_cpp(xfile):
    header = """#include "compatibility.h"

QList<QnCompatibilityItem> localCompatibilityItems()
{
    QList<QnCompatibilityItem> items;
"""

    footer = """
    return items;
}"""

    fill_compatibility_matrix()

    print >> xfile, header
    sorted_matrix = list(COMPATIBILITY_MATRIX)
    sorted_matrix.sort(key=lambda x: x.v1[1] * 100 + hash(x.comp1) + x.v2[1])

    for ci in sorted_matrix:
        print >> xfile, '    items.append(QnCompatibilityItem(QLatin1String("%s"), QLatin1String("%s"), QLatin1String("%s")));' % (version_to_string(ci.v1), ci.comp1, version_to_string(ci.v2))
    print >> xfile, footer

def gencomp_objc(xfile):
    header = """#import "HDWCompatibility.h"

NSArray *localCompatibilityItems()
{
    NSMutableArray *items = [NSMutableArray array];
"""

    footer = """
    return items;
}"""

    fill_compatibility_matrix()

    print >> xfile, header
    sorted_matrix = list(COMPATIBILITY_MATRIX)
    sorted_matrix.sort(key=lambda x: x.v1[1] * 100 + hash(x.comp1) + x.v2[1])

    for ci in sorted_matrix:
        print >> xfile, '    [items addObject:[HDWCompatibilityItem itemForVersion:@"%s" ofComponent:@"%s" andSystemVersion:@"%s"]];' % (version_to_string(ci.v1), ci.comp1, version_to_string(ci.v2))
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
    import argparse

    handlers = {'python': gencomp_py, 'c++': gencomp_cpp, 'objc': gencomp_objc}
    parser = argparse.ArgumentParser()
    parser.add_argument('lang', choices=['python', 'c++', 'objc'])
    parser.add_argument('outfile', type=argparse.FileType('w'), default=sys.stdout)
    args = parser.parse_args()
    handlers[args.lang](args.outfile)
    
    #gencomp_objc(sys.stdout)
#    gencomp_cpp(open('build/compatibility.cpp'))
