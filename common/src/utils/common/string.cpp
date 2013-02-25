#include "string.h"

#include <cmath>

#include "util.h"

QString replaceCharacters(const QString &string, const char *symbols, const QChar &replacement) {
    if(!symbols)
        return string;

    bool mask[256];
    memset(mask, 0, sizeof(mask));
    while(*symbols)
        mask[static_cast<int>(*symbols++)] = true; /* Static cast is here to silence GCC's -Wchar-subscripts. */

    QString result = string;
    for(int i = 0; i < result.size(); i++) {
        ushort c = result[i].unicode();
        if(c >= 256 || !mask[c])
            continue;

        result[i] = replacement;
    }

    return result;
}

QString formatFileSize(qint64 size, const QString pattern) {
    static const char *sizeSuffixes[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};

    QString number, suffix;
    if (size == 0) {
        number = lit("0");
        suffix = lit("B");
    } else {
        double absSize = std::abs(static_cast<double>(size));
        int power = static_cast<int>(std::log(absSize) / std::log(1000.0));
        int unit = power >= arraysize(sizeSuffixes) ? arraysize(sizeSuffixes) - 1 : power;

        suffix = lit(sizeSuffixes[unit]);
        number = (size < 0 ? lit("-") : QString()) + QString::number(absSize / std::pow(1000.0, unit), 'f', 1);

        /* Chop trailing zeros. */
        for(int i = number.size() - 1; ;i--) {
            QChar c = number[i];
            if(c != L'0' && c != L'.') {
                number.chop(number.size() - i - 1);
                break;
            }
        }

    }

    return pattern.arg(number).arg(suffix);
}
