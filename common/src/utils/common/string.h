#ifndef QN_STRING_H
#define QN_STRING_H

#include <QString>

/**
 * \param string                        String to perform replacement on.
 * \param symbols                       Symbols that are to be replaced.
 * \param replacement                   Character to use as a replacement.
 * \returns                             String with all characters from \a symbols replaced with \a replacement.
 */
inline QString replaceCharacters(const QString &string, const char *symbols, const QChar &replacement) {
    if(!symbols)
        return string;

    bool mask[256];
    memset(mask, 0, sizeof(mask));
    while(*symbols)
        mask[*symbols++] = true;

    QString result = string;
    for(int i = 0; i < result.size(); i++) {
        ushort c = result[i].unicode();
        if(c < 0 || c >= 256 || !mask[c])
            continue;

        result[i] = replacement;
    }

    return result;
}

inline QString replaceNonFileNameCharacters(const QString &string, const QChar &replacement) {
    return replaceAll(string, "\\/:*?\"<>|", replacement);
}

#endif // QN_STRING_H

