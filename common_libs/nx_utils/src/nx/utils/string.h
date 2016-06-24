#pragma once

#include <vector>
#include <memory>

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace nx {
namespace utils {

template <typename... Args>
std::string stringFormat(const std::string& format, Args... args)
{
    size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'.
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside.
}

enum MetricPrefix
{
    NoPrefix,
    KiloPrefix,
    MegaPrefix,
    GigaPrefix,
    TeraPrefix,
    PetaPrefix,
    ExaPrefix,
    ZettaPrefix,
    YottaPrefix,

    PrefixCount
};

/**
* \param size                          File size to format. Can be negative.
* \param precision                     Maximal number of decimal digits after comma.
* \param prefixThreshold               //TODO: #rvasilenko what is prefixThreshold? Why 1 is default, and usually passed 10?
* \param minPrefix
* \param maxPrefix
* \param useBinaryPrefixes
* \param pattern                       Pattern to use for result construction.
*                                      <tt>%1</tt> will be replaced with size in resulting units, and <tt>%2</tt> with unit name.
*/
NX_UTILS_API QString formatFileSize(qint64 size, int precision = 1, int prefixThreshold = 1, MetricPrefix minPrefix = NoPrefix, MetricPrefix maxPrefix = YottaPrefix, bool useBinaryPrefixes = true, const QString &pattern = lit("%1 %2"));

/**
* \param string                        String to perform replacement on.
* \param symbols                       Symbols that are to be replaced.
* \param replacement                   Character to use as a replacement.
* \returns                             String with all characters from \a symbols replaced with \a replacement.
*/
NX_UTILS_API QString replaceCharacters(const QString &string, const char *symbols, const QChar &replacement);

/**
* \param string                        String to perform replacement on.
* \param replacement                   Character to use as a replacement.
* \returns                             String with all non-filename characters replaces with \a replacement.
*/
inline QString replaceNonFileNameCharacters(const QString &string, const QChar &replacement)
{
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
NX_UTILS_API qint64 parseDateTime( const QString& dateTimeStr );

/*!
\param dateTime Can be one of following:\n
- usec or millis since since 1971-01-01 (not supporting 1970 to be able to distinguish millis and usec)
- date in ISO format (YYYY-MM-DDTHH:mm:ss)
- special value "now". In this case \a DATETIME_NOW is returned
- negative value. In this case value returned "as is"
\return msec since epoch
*/
NX_UTILS_API qint64 parseDateTimeMsec( const QString& dateTimeStr );

NX_UTILS_API int naturalStringCompare(const QString &lhs, const QString &rhs, Qt::CaseSensitivity caseSensitive = Qt::CaseSensitive, bool enableFloat = false);
NX_UTILS_API QStringList naturalStringSort(const QStringList &list, Qt::CaseSensitivity caseSensitive = Qt::CaseSensitive);

NX_UTILS_API bool naturalStringLess(const QString &lhs, const QString &rhs);

NX_UTILS_API QString xorEncrypt(const QString &plaintext, const QString &key);
NX_UTILS_API QString xorDecrypt(const QString &crypted, const QString &key);

/** Returns filename extension (with dot) from the string containing filename of dialog filter.
*  Supports strings like "/home/file.avi" -> ".avi"; "Avi files (*.avi)" -> ".avi"
*  Returns an empty string if the string does not contain any dot.
*/
NX_UTILS_API QString extractFileExtension(const QString &string);

/** Returns string formed as "baseValue<spacer>(n)" that is not contained in the usedValues list. */
NX_UTILS_API QString generateUniqueString(const QStringList &usedStrings, const QString &defaultString, const QString &templateString);

NX_UTILS_API void trimInPlace( QString* const str, const QString& symbols = QLatin1String(" ") );

NX_UTILS_API QString elideString(const QString &source, int maxLength, const QString &tail = lit("..."));

//!Generates random string containing only letters and digits
NX_UTILS_API QByteArray generateRandomName(int length);

/** Converts \a bytes to human-readable string like 10M or 67K.
 * Rounds value to floor with the given float precision.
*/
NX_UTILS_API QString bytesToString(uint64_t bytes, int precision = 4);

/** Converts bytes number string (e.g., 64K) to integer.
K, M, G suffix are supported.
@return On failure returns 0 and (if not null) sets \a *ok to \a false
*/
NX_UTILS_API uint64_t stringToBytes(const QString& str, bool* isOk = nullptr);

} // namespace utils
} // namespace nx




