#pragma once

#include <cctype>
#include <memory>
#include <vector>

#include <QtCore/QDateTime>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "literal.h"
#include "qnbytearrayref.h"

namespace nx::utils {

/**
* \param string                        String to perform replacement on.
* \param symbols                       Symbols that are to be replaced.
* \param replacement                   Character to use as a replacement.
* \returns                             String with all characters from symbols replaced with replacement.
*/
NX_UTILS_API QString replaceCharacters(const QString &string, const char *symbols, const QChar &replacement);

/**
* \param string                        String to perform replacement on.
* \param replacement                   Character to use as a replacement.
* \returns                             String with all non-filename characters replaces with replacement.
*/
inline QString replaceNonFileNameCharacters(const QString &string, const QChar &replacement)
{
    return replaceCharacters(string, "\\/:*?\"<>|", replacement);
}

/*!
\param dateTime Can be one of following:\n
- usec or millis since since 1971-01-01 (not supporting 1970 to be able to distinguish millis and usec)
- date in ISO format (YYYY-MM-DDTHH:mm:ss)
- special value "now". In this case DATETIME_NOW is returned
- negative value. In this case value returned "as is"
\return usec since epoch
*/
NX_UTILS_API qint64 parseDateTime( const QString& dateTimeStr );

/*!
\param dateTime Can be one of following:\n
- usec or millis since since 1971-01-01 (not supporting 1970 to be able to distinguish millis and usec)
- date in ISO format (YYYY-MM-DDTHH:mm:ss)
- special value "now". In this case DATETIME_NOW is returned
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

NX_UTILS_API QString elideString(
    const QString& source, int maxLength, const QString& tail = QLatin1String("..."));

/**
 * Replace several substrings in the source string at once. Handles situation when one of
 * substituted values contains source of another substitution. So if source string was 'abc' and
 * you want to replace 'a->b' and 'b->c', this algorithm will produce 'bcc' string, while
 * consequent replaces will produce 'ccc'. When several substitutions can be applied, first one
 * is selected.
 */
NX_UTILS_API QString replaceStrings(
    const QString& source,
    const std::vector<std::pair<QString, QString>>& substitutions,
    Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive);

//!Generates random string containing only letters and digits
NX_UTILS_API QByteArray generateRandomName(int length);

/**
 * Converts bytes to human-readable string like 10M or 67K.
 * Rounds value to floor with the given float precision.
 * Should be used only for internal development and/or logs. All user-visible strings must be
 * generated using HumanReadable class methods (e.g. digitalSize()).
 */
NX_UTILS_API QString bytesToString(uint64_t bytes, int precision = 4);

/** Converts bytes number string (e.g., 64K) to integer.
K, M, G suffix are supported.
@return On failure returns 0 and (if not null) sets *ok to false
*/
NX_UTILS_API uint64_t stringToBytes(const QString& str, bool* isOk = nullptr);
NX_UTILS_API uint64_t stringToBytes(const QString& str, uint64_t defaultValue);
NX_UTILS_API uint64_t stringToBytesConst(const char* str);

enum GroupToken
{
    doubleQuotes = 1,
    squareBraces = 2,
    roundBraces = 4,
    singleQuotes = 8,
};

/**
 * Splits src string to substring using separator.
 * Separators inside group tokens (inside double quotes by default) are ignored.
 * @param groupTokens Bitset of group tokens.
 */
NX_UTILS_API std::vector<QnByteArrayConstRef> splitQuotedString(
    const QnByteArrayConstRef& src,
    char separator=',',
    int groupTokens = GroupToken::doubleQuotes);

template<template<typename...> class Dictionary>
void parseNameValuePairs(
    const QnByteArrayConstRef& serializedData,
    char separator,
    Dictionary<QByteArray, QByteArray>* const params,
    int groupTokens = GroupToken::doubleQuotes)
{
    const std::vector<QnByteArrayConstRef>& paramsList =
        splitQuotedString(serializedData, separator, groupTokens);
    for (const QnByteArrayConstRef& token: paramsList)
    {
        const auto& nameAndValue = splitQuotedString(token.trimmed(), '=', groupTokens);
        if (nameAndValue.empty())
            continue;
        QnByteArrayConstRef value = nameAndValue.size() > 1 ? nameAndValue[1] : QnByteArrayConstRef();
        params->emplace(nameAndValue[0].trimmed(), value.trimmed("\""));
    }
}

/**
 * Parses string like "name1=value1,name2=value2,...".
 */
template<template<typename...> class Dictionary>
Dictionary<QByteArray, QByteArray> parseNameValuePairs(
    const QnByteArrayConstRef& serializedData,
    char separator = ',',
    int groupTokens = GroupToken::doubleQuotes)
{
    Dictionary<QByteArray, QByteArray> nameValueContainer;
    parseNameValuePairs<Dictionary>(
        serializedData,
        separator,
        &nameValueContainer,
        groupTokens);
    return nameValueContainer;
}

/**
 * Serializes dictionary of (name, value) pairs into string like "name1=value1,name2=value2,...".
 */
template<template<typename...> class Dictionary>
QByteArray serializeNameValuePairs(
    const Dictionary<QByteArray, QByteArray>& params)
{
    QByteArray serializedData;
    serializeNameValuePairs<Dictionary>(params, &serializedData);
    return serializedData;
}

template<template<typename...> class Dictionary>
void serializeNameValuePairs(
    const Dictionary<QByteArray, QByteArray>& params,
    QByteArray* const dstBuffer)
{
    for (auto it = params.cbegin(); it != params.cend(); ++it)
    {
        if (it != params.begin())
            dstBuffer->append(", ");
        dstBuffer->append(it->first);
        dstBuffer->append("=\"");
        dstBuffer->append(it->second);
        dstBuffer->append("\"");
    }
}

/**
 * Removes all ampersands that are not concatenated with others
 * and not followed by whitespace.
 */
NX_UTILS_API QString removeMnemonics(QString text);

//!Splits data by delimiter not closed within quotes
/*!
    E.g.:
    \code
    one, two, "three, four"
    \endcode

    will be splitted to
    \code
    one
    two
    "three, four"
    \endcode
*/
NX_UTILS_API QList<QByteArray> smartSplit(
    const QByteArray& data,
    const char delimiter);

NX_UTILS_API QStringList smartSplit(
    const QString& data,
    const QChar delimiter,
    QString::SplitBehavior splitBehavior = QString::KeepEmptyParts);

NX_UTILS_API QByteArray unquoteStr(const QByteArray& v);

NX_UTILS_API QString unquoteStr(const QString& v);

static const size_t BufferNpos = size_t(-1);

/**
 * Searches first occurence of any element of \0-terminated string toSearch in count elements of str,
 *   starting with element offset.
 * @param toSearch \0-terminated string.
 * @param count number of characters of str to check,
 *   NOT a length of searchable characters string (toSearch).
 * NOTE: following algorithms differ from stl analogue in following:
 *   they limit number of characters checked during search.
 */
template<class Str>
size_t find_first_of(
    const Str& str,
    const typename Str::value_type* toSearch,
    size_t offset = 0,
    size_t count = BufferNpos)
{
    const size_t toSearchDataSize = strlen(toSearch);
    const typename Str::value_type* strEnd = str.data() + (count == BufferNpos ? str.size() : (offset + count));
    for (const typename Str::value_type*
        curPos = str.data() + offset;
        curPos < strEnd;
        ++curPos)
    {
        if (memchr(toSearch, *curPos, toSearchDataSize))
            return curPos - str.data();
    }

    return BufferNpos;
}

template<class Str>
size_t find_first_not_of(
    const Str& str,
    const typename Str::value_type* toSearch,
    size_t offset = 0,
    size_t count = BufferNpos)
{
    const size_t toSearchDataSize = strlen(toSearch);
    const typename Str::value_type* strEnd = str.data() + (count == BufferNpos ? str.size() : (offset + count));
    for (const typename Str::value_type*
        curPos = str.data() + offset;
        curPos < strEnd;
        ++curPos)
    {
        if (memchr(toSearch, *curPos, toSearchDataSize) == NULL)
            return curPos - str.data();
    }

    return BufferNpos;
}

template<class Str>
size_t find_last_not_of(
    const Str& str,
    const typename Str::value_type* toSearch,
    size_t offset = 0,
    size_t count = BufferNpos)
{
    const size_t toSearchDataSize = strlen(toSearch);
    const typename Str::value_type* strEnd = str.data() + (count == BufferNpos ? str.size() : (offset + count));
    for (const typename Str::value_type*
        curPos = strEnd - 1;
        curPos >= str.data();
        --curPos)
    {
        if (memchr(toSearch, *curPos, toSearchDataSize) == NULL)
            return curPos - str.data();
    }

    return BufferNpos;
}

template<class Str, class StrValueType>
size_t find_last_of(
    const Str& str,
    const StrValueType toSearch,
    size_t offset = 0,
    size_t count = BufferNpos)
{
    const StrValueType* strEnd = str.data() + (count == BufferNpos ? str.size() : (offset + count));
    for (const StrValueType*
        curPos = strEnd - 1;
        curPos >= str.data();
        --curPos)
    {
        if (toSearch == *curPos)
            return curPos - str.data();
    }

    return BufferNpos;
}

template<template<typename...> class String, typename CharType>
// requires Random_access_iterator<typename String<Char>::iterator>
String<CharType> maxPrefix(const String<CharType>& one, const String<CharType>& two)
{
    auto oneIter = one.begin();
    auto twoIter = two.begin();

    while ((oneIter != one.end()) && (twoIter != two.end()) && (*oneIter == *twoIter))
    {
        ++oneIter;
        ++twoIter;
    }

    return String<CharType>(one.begin(), oneIter);
}

/**
 * Format Json string with indentation to make it human-readable.
 */
NX_UTILS_API QByteArray formatJsonString(const QByteArray& data);

NX_UTILS_API int stricmp(const std::string& left, const std::string& right);

} // namespace nx::utils
