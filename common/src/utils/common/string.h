#ifndef QN_STRING_H
#define QN_STRING_H

#include <QString>

/**
 * \param string                        String to perform replacement on.
 * \param symbols                       Symbols that are to be replaced.
 * \param replacement                   Character to use as a replacement.
 * \returns                             String with all characters from \a symbols replaced with \a replacement.
 */
QString replaceCharacters(const QString &string, const char *symbols, const QChar &replacement);

inline QString replaceNonFileNameCharacters(const QString &string, const QChar &replacement) {
    return replaceCharacters(string, "\\/:*?\"<>|", replacement);
}

/**
 * \param size                          File size to format. Can be negative.
 * \param pattern                       Pattern to use for result construction. 
                                        <tt>%1</tt> will be replaced with size in resulting units, and <tt>%2</tt> with unit name.
 */
QString formatFileSize(qint64 size, const QString pattern = lit("%1 %2"));

#endif // QN_STRING_H

