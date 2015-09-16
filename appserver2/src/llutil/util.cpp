#include "util.h"

QString LLUtil::changedGuidByteOrder(const QString& guid)
{
    Q_ASSERT(guid.length() == 36);

    QString result(guid);

    int replace[] = {6, 7, 4, 5, 2, 3, 0, 1, 8, 11, 12, 9, 10, 13, 16, 17, 14, 15};
    
    for (int i = 0; i < sizeof(replace) / sizeof(replace[0]); i++) {
        result[i] = guid[replace[i]];
    }

    return result;
}
