// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string_view>
#include <vector>

#include <QtCore/QDateTime>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "literal.h"
#include "qnbytearrayref.h"
#include "std_string_utils.h"

namespace nx::utils {

/**
* \param string                        String to perform replacement on.
* \param symbols                       Symbols that are to be replaced.
* \param replacement                   Character to use as a replacement.
* \returns                             String with all characters from symbols replaced with replacement.
*/
NX_UTILS_API QString replaceCharacters(
    const QString &string,
    std::string_view symbols,
    const QChar &replacement);

/**
* \param string                        String to perform replacement on.
* \param replacement                   Character to use as a replacement.
* \returns                             String with all non-filename characters replaces with replacement.
*/
inline QString replaceNonFileNameCharacters(const QString &string, const QChar &replacement)
{
    return replaceCharacters(
        string, std::string_view("/\0\t\n\\:*?\"<>|", 12), replacement).trimmed();
}

/*!
Like QString::toInt, but throws on failure.
*/
NX_UTILS_API int parseInt(const QString& string, int base = 10);

/*!
Like QString::toDouble, but throws on failure.
*/
NX_UTILS_API double parseDouble(const QString& string);

NX_UTILS_API int naturalStringCompare(
    QStringView lhs,
    QStringView rhs,
    Qt::CaseSensitivity caseSensitive = Qt::CaseSensitive,
    bool enableFloat = false);
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
NX_UTILS_API std::string generateRandomName(int length);

/**
 * Converts bytes to human-readable string like 10M or 67K.
 * Rounds value to floor with the given float precision.
 * Should be used only for internal development and/or logs. All user-visible strings must be
 * generated using HumanReadable class methods (e.g. digitalSize()).
 */
NX_UTILS_API std::string bytesToString(uint64_t bytes, int precision = 4);

/**
 * Converts bytes number string (e.g., 64K) to integer.
 * K, M, G suffix are supported.
 * @return On failure returns 0 and (if not null) sets *ok to false.
*/
NX_UTILS_API uint64_t stringToBytes(const std::string_view& str, bool* ok = nullptr);
NX_UTILS_API uint64_t stringToBytes(const std::string_view& str, uint64_t defaultValue);

/**
 * Removes all ampersands that are not concatenated with others
 * and not followed by whitespace.
 */
NX_UTILS_API QString removeMnemonics(QString text);

/**
 * Escapes all mnemonic characters in the string. Each ampersand in the string is doubled.
 */
NX_UTILS_API QString escapeMnemonics(QString text);

//!Splits data by delimiter not closed within quotes
/*!
    E.g.:
    \code
    one, two, "three, four"
    \endcode

    will be split to
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
    Qt::SplitBehavior splitBehavior = Qt::KeepEmptyParts);

template <class T, class T2>
T unquote(const T& str, T2 quoteChar)
{
    int pos1 = str.startsWith(T2(quoteChar)) ? 1 : 0;
    int pos2 = str.endsWith(T2(quoteChar)) ? 1 : 0;
    return str.mid(pos1, str.length() - pos1 - pos2);
}

template <class T, class T2>
T trimAndUnquote(const T& str, T2 quoteChar)
{
    return unquote(str.trimmed(), quoteChar);
}

NX_UTILS_API QByteArray trimAndUnquote(const QByteArray& v);

NX_UTILS_API QString trimAndUnquote(const QString& v);

static const size_t BufferNpos = size_t(-1);

/**
 * Searches the first occurrence of any element of \0-terminated string toSearch in count elements
 *   of str, starting with element offset.
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

/**
 * Format Json string with indentation to make it human-readable.
 */
NX_UTILS_API QByteArray formatJsonString(const QByteArray& data);

NX_UTILS_API nx::Buffer formatJsonString(const nx::Buffer& data);

/**
* Concatenate all strings from a specified container, inserting specified separator between them.
*/
// TODO: VMS-20167 Combine with nx::utils::join.
template<
    typename StringContainerType,
    typename SeparatorType,
    typename StringType = typename StringContainerType::value_type>
StringType strJoin(const StringContainerType& strings, const SeparatorType& separator)
{
    if (strings.size() == 0)
        return StringType();

    StringType result;
    const StringType separatorStr(separator);

    using SizeType = decltype(result.size());

    const auto accumulatedSize = separatorStr.size() * (strings.size() - 1)
        + std::accumulate(strings.begin(), strings.end(), SizeType(0),
            [](SizeType value, const StringType& string) { return value + string.size(); });

    result.reserve(accumulatedSize);

    auto it = strings.begin();
    result += *it;

    for (++it; it != strings.end(); ++it)
    {
        result += separatorStr;
        result += *it;
    }

    return result;
}

/**
* Concatenate all strings from a specified range, inserting specified separator between them.
*/
template<
    typename BeginType,
    typename EndType,
    typename SeparatorType,
    typename StringType = std::remove_cv_t<std::remove_reference_t<decltype(**((BeginType*)0))>>>
StringType strJoin(BeginType begin, EndType end, const SeparatorType& separator)
{
    StringType result;
    const StringType separatorStr(separator);

    auto it = begin;
    result += *it;

    for (++it; it != end; ++it)
    {
        result += separatorStr;
        result += *it;
    }

    return result;
}

/**
* Truncate QString to the first '\x00' character.
*/
NX_UTILS_API void truncateToNul(QString* s);

/**
* Convert any buffer to hex string.
*/
NX_UTILS_API std::string toHex(
    const void* buffer, const int size, const std::string& delimeter = " ");

/**
 * Hides the second half of the string
 */
NX_UTILS_API std::string half(const std::string& str);

} // namespace nx::utils
