#include "util.h"

namespace nx {
namespace utils {
namespace license {

QString changedGuidByteOrder(const QString& guid)
{
    if (guid.length() != 36)
        return guid;

    QString result(guid);

    int replace[] = {6, 7, 4, 5, 2, 3, 0, 1, 8, 11, 12, 9, 10, 13, 16, 17, 14, 15};
    
    for (size_t i = 0; i < sizeof(replace) / sizeof(replace[0]); i++)
        result += guid[replace[i]];

    return result;
}

int hardwareIdVersion(const QString& hardwareId)
{
    if (hardwareId.length() == 32)
        return 0;

    if (hardwareId.length() == 34)
        return hardwareId.mid(0, 2).toInt();

    return -1;
}

} // namespace license
} // namespace utils
} // namespace nx
