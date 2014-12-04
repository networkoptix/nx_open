#ifndef QN_STRING_H
#define QN_STRING_H

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace Qn {
    enum MetricPrefix {
        NoPrefix,
        KiloPrefix,
        MegaPrefix,
        GigaPrefix,
        TeraPrefix,
        PetaPrefix,
        ExaPrefix,
        ZettaPrefix,
        YottaPrefix
    };

} // namespace Qn


/**
 * \param string                        String to perform replacement on.
 * \param symbols                       Symbols that are to be replaced.
 * \param replacement                   Character to use as a replacement.
 * \returns                             String with all characters from \a symbols replaced with \a replacement.
 */
QString replaceCharacters(const QString &string, const char *symbols, const QChar &replacement);

/**
 * \param string                        String to perform replacement on.
 * \param replacement                   Character to use as a replacement.
 * \returns                             String with all non-filename characters replaces with \a replacement.
 */
inline QString replaceNonFileNameCharacters(const QString &string, const QChar &replacement) {
    return replaceCharacters(string, "\\/:*?\"<>|", replacement);
}

/**
 * \param dt                            dateTime
 * \returns                             Return string dateTime suggestion for saving dialogs
 */
inline QString datetimeSaveDialogSuggestion(const QDateTime& dt) {
    return dt.toString(lit("yyyy-MMM-dd_hh.mm.ss"));
}

/**
 * \param size                          File size to format. Can be negative.
 * \param precision                     Maximal number of decimal digits after comma.
 * \param prefixThreshold               
 * \param minPrefix
 * \param maxPrefix
 * \param useBinaryPrefixes
 * \param pattern                       Pattern to use for result construction. 
 *                                      <tt>%1</tt> will be replaced with size in resulting units, and <tt>%2</tt> with unit name.
 */
QString formatFileSize(qint64 size, int precision = 1, int prefixThreshold = 1, Qn::MetricPrefix minPrefix = Qn::NoPrefix, Qn::MetricPrefix maxPrefix = Qn::YottaPrefix, bool useBinaryPrefixes = true, const QString pattern = QLatin1String("%1 %2"));

int naturalStringCompare(const QString &lhs, const QString &rhs, Qt::CaseSensitivity caseSensitive = Qt::CaseSensitive , bool enableFloat = true );
QStringList naturalStringSort(const QStringList &list, Qt::CaseSensitivity caseSensitive = Qt::CaseSensitive);

bool naturalStringLessThan(const QString &lhs, const QString &rhs);
bool naturalStringCaseInsensitiveLessThan(const QString &lhs, const QString &rhs);

void naturalStringCompareTest();

QString xorEncrypt(const QString &plaintext, const QString &key);
QString xorDecrypt(const QString &crypted, const QString &key);

/** Returns filename extension (with dot) from the string containing filename of dialog filter.
 *  Supports strings like "/home/file.avi" -> ".avi"; "Avi files (*.avi)" -> ".avi"
 *  Returns an empty string if the string does not contain any dot.
 */
QString extractFileExtension(const QString &string);

/** Returns string formed as "baseValue<spacer>(n)" that is not contained in the usedValues list. */
QString generateUniqueString(const QStringList &usedStrings, const QString &defaultString, const QString &templateString);

void trimInPlace( QString* const str, const QString& symbols = QLatin1String(" ") );


#endif // QN_STRING_H
