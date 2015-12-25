#ifndef QN_STRING_H
#define QN_STRING_H

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "common/common_globals.h"


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
    return dt.toString(lit("yyyy-MMM-dd_hh_mm_ss"));
}

/*!
    \param dateTime Can be one of following:\n
        - usec or millis since since 1971-01-01 (not supporting 1970 to be able to distinguish millis and usec)
        - date in ISO format (YYYY-MM-DDTHH:mm:ss)
        - special value "now". In this case \a DATETIME_NOW is returned
        - negative value. In this case value returned "as is"
    \return usec since epoch
*/
qint64 parseDateTime( const QString& dateTimeStr );


/*!
    \param dateTime Can be one of following:\n
        - usec or millis since since 1971-01-01 (not supporting 1970 to be able to distinguish millis and usec)
        - date in ISO format (YYYY-MM-DDTHH:mm:ss)
        - special value "now". In this case \a DATETIME_NOW is returned
        - negative value. In this case value returned "as is"
    \return msec since epoch
*/
qint64 parseDateTimeMsec( const QString& dateTimeStr );

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
QString formatFileSize(qint64 size, int precision = 1, int prefixThreshold = 1, Qn::MetricPrefix minPrefix = Qn::NoPrefix, Qn::MetricPrefix maxPrefix = Qn::YottaPrefix, bool useBinaryPrefixes = true, const QString &pattern = QLatin1String("%1 %2"));

int naturalStringCompare(const QString &lhs, const QString &rhs, Qt::CaseSensitivity caseSensitive = Qt::CaseSensitive, bool enableFloat = false);
QStringList naturalStringSort(const QStringList &list, Qt::CaseSensitivity caseSensitive = Qt::CaseSensitive);

bool naturalStringLess(const QString &lhs, const QString &rhs);

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

QString htmlBold(const QString &source);
QString htmlFormattedParagraph(const QString &text
    , int pixelSize
    , bool isBold = false
    , bool isItalic = false);


QString elideString(const QString &source, int maxLength, const QString &tail = lit("..."));

//!Generates random string containing only letters and digits
QByteArray generateRandomName(int length);

QString elideHtml(const QString &html, int maxLength, const QString &tail = lit("..."));

#endif // QN_STRING_H
