// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "string.h"

#include <array>
#include <cmath>
#include <string>

#include <QtCore/QByteArray>
#include <QtCore/QRegularExpression>

#include <nx/utils/log/assert.h>
#include <nx/utils/datetime.h>
#include <nx/utils/random.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/exception.h>

namespace nx::utils {

QString replaceCharacters(
    const QString &string,
    std::string_view symbols,
    const QChar &replacement)
{
    if (symbols.empty())
        return string;

    bool mask[256];
    memset(mask, 0, sizeof(mask));
    for (const auto s: symbols)
        mask[static_cast<int>(s)] = true; /* Static cast is here to silence GCC's -Wchar-subscripts. */

    QString result = string;
    for (int i = 0; i < result.size(); i++)
    {
        const ushort c = result[i].unicode();
        if (c >= 256 || !mask[c])
            continue;

        result[i] = replacement;
    }

    return result;
}

qint64 parseDateTime(const QString& dateTimeStr)
{
    static const qint64 MIN_PER_HOUR = 60;
    static const qint64 SEC_PER_MIN = 60;
    static const qint64 HOUR_PER_DAY = 24;
    static const qint64 DAY_PER_NON_LEAP_YEAR = 365;
    static const qint64 MS_PER_SEC = 1000;
    static const qint64 USEC_PER_MS = 1000;

    if (dateTimeStr.toLower().trimmed() == "now")
    {
        return DATETIME_NOW;
    }
    else if (dateTimeStr.contains('T') || (dateTimeStr.contains('-') && !dateTimeStr.startsWith('-')))
    {
        QDateTime tmpDateTime = QDateTime::fromString(trimAndUnquote(dateTimeStr), Qt::ISODateWithMs);
        return tmpDateTime.toMSecsSinceEpoch() * USEC_PER_MS;
    }
    else
    {
        auto somethingSinceEpoch = dateTimeStr.toLongLong();
        //detecting millis or usec?
        if (somethingSinceEpoch > 0 && somethingSinceEpoch < (DAY_PER_NON_LEAP_YEAR * HOUR_PER_DAY * MIN_PER_HOUR * SEC_PER_MIN * MS_PER_SEC * USEC_PER_MS))
            return somethingSinceEpoch * USEC_PER_MS;   //dateTime is in millis
        else
            return somethingSinceEpoch;
    }
}

qint64 parseDateTimeMsec(const QString& dateTimeStr)
{
    static const qint64 kUsecPerMs = 1000;

    qint64 usecSinceEpoch = parseDateTime(dateTimeStr);
    if (usecSinceEpoch < 0 || usecSinceEpoch == DATETIME_NOW)
        return usecSinceEpoch;  //special values are returned "as is"

    return usecSinceEpoch / kUsecPerMs;
}

int parseInt(const QString& string, int base)
{
    int value;
    if (bool ok; (value = string.toInt(&ok, base)), !ok)
    {
        if (base == 10)
            throw ContextedException("Failed to parse int: %1", string);
        else
            throw ContextedException("Failed to parse base-%1 int: %2", base, string);
    }
    return value;
}

double parseDouble(const QString& string)
{
    int value;
    if (bool ok; (value = string.toDouble(&ok)), !ok)
        throw ContextedException("Failed to parse double: %1", string);
    return value;
}

QString xorEncrypt(const QString &plaintext, const QString &key)
{
    if (key.isEmpty())
        return plaintext;

    QByteArray array(plaintext.toUtf8());
    QByteArray keyArray(key.toUtf8());

    for (int i = 0; i < array.size(); i++)
        array[i] = array[i] ^ keyArray[i % keyArray.size()];

    return QLatin1String(array.toBase64());
}

QString xorDecrypt(const QString &crypted, const QString &key)
{
    if (key.isEmpty())
        return crypted;

    QByteArray array = QByteArray::fromBase64(crypted.toLatin1());
    QByteArray keyArray(key.toUtf8());

    for (int i = 0; i < array.size(); i++)
        array[i] = array[i] ^ keyArray[i % keyArray.size()];
    return QString::fromUtf8(array);
}


QString extractFileExtension(const QString &string)
{
    auto pos = string.lastIndexOf('.');
    if (pos < 0)
        return QString();

    QString result('.');
    while (++pos < string.length())
    {
        QChar curr = string[pos];
        if (!curr.isLetterOrNumber())
            return result;
        result.append(curr);
    }

    return result;
}


QString generateUniqueString(
    const QStringList& usedStrings,
    const QString& defaultString,
    const QString& templateString)
{
    QStringList lowerStrings;
    for (const QString &string : usedStrings)
        lowerStrings << string.toLower();

    const QString escapedTemplate = replaceStrings(templateString,
        {{"(", "\\("}, {")", "\\)"}});
    const QString stringPattern = escapedTemplate.arg("?([0-9]+)?").toLower();
    auto pattern = QRegularExpression(QRegularExpression::anchoredPattern(stringPattern));

    /* Prepare new name. */
    int number = 0;
    for (const QString &string : lowerStrings)
    {
        auto result = pattern.match(string);
        if (!result.hasMatch())
            continue;

        number = qMax(number, result.capturedView(1).toInt());
    }

    if (number == 0)
    {
        if (defaultString.isEmpty())
        {
            number = 1;
        }
        else if (!lowerStrings.contains(defaultString.toLower()))
        {
            return defaultString;
        }
        else
        {
            number = 2;
        }
    }
    else
    {
        number++;
    }

    return templateString.arg(number);
}



// -------------------------------------------------------------------------- //
// String comparison
// -------------------------------------------------------------------------- //
/*
 * This software was written by people from OnShore Consulting services LLC
 * <info@sabgroup.com> and placed in the public domain.
 *
 * We reserve no legal rights to any of this. You are free to do
 * whatever you want with it. And we make no guarantee or accept
 * any claims on damages as a result of this.
 *
 * If you change the software, please help us and others improve the
 * code by sending your modifications to us. If you choose to do so,
 * your changes will be included under this license, and we will add
 * your name to the list of contributors.
 */
#define INCBUF() { buffer += curr; ++pos; curr = ( pos < string.length() ) ? string[ pos ] : QChar(); }

bool isNumberStart(const QChar &c)
{
/* We don't want to handle negative numbers as this leads to very strange
 * results. Think how "1-1" and "1-2" are going to be compared in this
 * case. */

    return
#if 0
        c == L'-' || c == L'+' ||
#endif
        c.isDigit();
}

void ExtractToken(QString & buffer, const QString & string, int & pos, bool & isNumber, bool enableFloat)
{
    buffer.clear();
    if (string.isNull() || pos >= string.length())
        return;

    isNumber = false;
    QChar curr = string[pos];
    // TODO:: Fix it
    // If you don't want to handle sign of the number, this isNumberStart is not needed indeed
    if (isNumberStart(curr))
    {
#if 0
        if (curr == L'-' || curr == L'+')
            INCBUF();
#endif

        if (!curr.isNull() && curr.isDigit())
        {
            isNumber = true;
            while (curr.isDigit())
                INCBUF();

            if (curr == '.')
            {
                if (enableFloat)
                {
                    INCBUF();
                    while (curr.isDigit())
                        INCBUF();
                }
                else
                {
                             // We are done since we meet first character that is not expected
                    return;
                }
            }

            /* We don't want to handle exponential notation.
             * Besides, this implementation is buggy as it treats '14easd'
             * as a number. */
#if 0
            if (!curr.isNull() && curr.toLower() == L'e')
            {
                INCBUF();
                if (curr == L'-' || curr == L'+')
                    INCBUF();

                if (curr.isNull() || !curr.isDigit())
                    isNumber = false;
                else
                    while (curr.isDigit())
                        INCBUF();
            }
#endif
        }
    }

    if (!isNumber)
    {
        while (!isNumberStart(curr) && pos < string.length())
            INCBUF();
    }
}

int naturalStringCompare(const QString & lhs, const QString & rhs, Qt::CaseSensitivity caseSensitive, bool enableFloat)
{
    int ii = 0;
    int jj = 0;

    QString lhsBufferQStr;
    QString rhsBufferQStr;

    int retVal = 0;

    // all status values are created on the stack outside the loop to make as fast as possible
    bool lhsNumber = false;
    bool rhsNumber = false;

    double lhsValue = 0.0;
    double rhsValue = 0.0;
    bool ok1;
    bool ok2;

    while (retVal == 0 && ii < lhs.length() && jj < rhs.length())
    {
        ExtractToken(lhsBufferQStr, lhs, ii, lhsNumber, enableFloat);
        ExtractToken(rhsBufferQStr, rhs, jj, rhsNumber, enableFloat);

        if (!lhsNumber && !rhsNumber)
        {
            // both strings curr val is a simple strcmp
            retVal = lhsBufferQStr.compare(rhsBufferQStr, caseSensitive);

            int maxLen = qMin(lhsBufferQStr.length(), rhsBufferQStr.length());
            QString tmpRight = rhsBufferQStr.left(maxLen);
            QString tmpLeft = lhsBufferQStr.left(maxLen);
            if (tmpLeft.compare(tmpRight, caseSensitive) == 0)
            {
                retVal = lhsBufferQStr.length() - rhsBufferQStr.length();

                if (retVal < 0)
                {
                    if (ii < lhs.length() && isNumberStart(lhs[ii]))
                        retVal *= -1;
                }
                else if (retVal > 0)
                {
                    if (jj < rhs.length() && isNumberStart(rhs[jj]))
                        retVal *= -1;
                }
            }
        }
        else if (lhsNumber && rhsNumber)
        {
            // both numbers, convert and compare
            lhsValue = lhsBufferQStr.toDouble(&ok1);
            rhsValue = rhsBufferQStr.toDouble(&ok2);
            if (!ok1 || !ok2)
                retVal = lhsBufferQStr.compare(rhsBufferQStr, caseSensitive);
            else if (lhsValue > rhsValue)
                retVal = 1;
            else if (lhsValue < rhsValue)
                retVal = -1;
        }
        else
        {
            // completely arebitrary that a number comes before a string
            retVal = lhsNumber ? -1 : 1;
        }
    }

    if (retVal != 0)
        return retVal;
    if (ii < lhs.length())
        return 1;
    else if (jj < rhs.length())
        return -1;
    else
        return 0;
}

bool naturalStringLess(const QString &lhs, const QString &rhs)
{
    return naturalStringCompare(lhs, rhs, Qt::CaseInsensitive) < 0;
}

QStringList naturalStringSort(const QStringList &list, Qt::CaseSensitivity caseSensitive)
{
    QStringList result = list;
    if (caseSensitive == Qt::CaseSensitive)
        std::sort(result.begin(), result.end(), [](const QString &left, const QString &right)
    {
        return naturalStringCompare(left, right, Qt::CaseSensitive) < 0;
    });
    else
        std::sort(result.begin(), result.end(), naturalStringLess);
    return result;
}

void trimInPlace(QString* const str, const QString& symbols)
{
    int startPos = 0;
    for (; startPos < str->size(); ++startPos)
    {
        if (!symbols.contains(str->at(startPos)))
            break;
    }

    int endPos = str->size() - 1;
    for (; endPos >= 0; --endPos)
    {
        if (!symbols.contains(str->at(endPos)))
            break;
    }
    ++endPos;

    *str = str->mid(startPos, endPos > startPos ? (endPos - startPos) : 0);
}

QString elideString(const QString &source, int maxLength, const QString &tail)
{
    if (source.length() <= maxLength)
        return source;

    const auto tailLength = tail.length();
    const auto elidedText = source.left(maxLength > tailLength ? maxLength - tailLength : 0);
    return (elidedText + tail);
}

QString replaceStrings(
    const QString& source,
    const std::vector<std::pair<QString, QString>>& substitutions,
    Qt::CaseSensitivity caseSensitivity)
{
    if (substitutions.empty() || source.isEmpty())
        return source;

    QHash<QString, QString> replacements;
    QString pattern;
    for (const auto& [src, dst]: substitutions)
    {
        if (!replacements.contains(src))
            replacements[src] = dst;

        pattern.append(QRegularExpression::escape(src));
        pattern.append('|');
    }
    pattern.chop(1);

    QRegularExpression re(pattern,
        caseSensitivity == Qt::CaseInsensitive
            ? QRegularExpression::CaseInsensitiveOption
            : QRegularExpression::NoPatternOption);

    QString result;

    int lastPos = 0;
    auto it = re.globalMatch(source);
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        result += source.mid(lastPos, match.capturedStart() - lastPos);
        result += replacements.value(match.captured());
        lastPos = match.capturedEnd();
    }
    result += source.mid(lastPos);

    return result;
}

std::string generateRandomName(int length)
{
    return random::generateName(length);
}

static const double kByteSuffixLimit = 1024;
static const std::vector<char> kByteSuffexes = { 'K', 'M', 'G', 'T' };

// TODO: #sivanov Move out strings and logic to separate class.
std::string bytesToString(uint64_t bytes, int precision)
{
    double number = static_cast<double>(bytes);
    size_t suffix = 0;

    while (number >= kByteSuffixLimit
           && suffix < kByteSuffexes.size())
    {
        number /= kByteSuffixLimit;
        ++suffix;
    }

    // TODO: #akolesnikov Use std::to_chars when available on every platform.

    if (suffix == 0)
        return nx::format("%1").arg(number, 0, 'g', precision).toStdString();

    return nx::format("%1%2").arg(number, 0, 'g', precision).arg(kByteSuffexes[suffix - 1]).toStdString();
}

uint64_t stringToBytes(const std::string_view& str, bool* isOk)
{
    if (str.empty())
    {
        if (isOk)
            *isOk = false;
        return 0;
    }

    const auto subDouble = [isOk](const std::string_view& str, const auto& multi)
    {
        std::size_t endPos = 0;
        const auto result = static_cast<uint64_t>(stod(str, &endPos) * multi);
        if (isOk)
            *isOk = endPos > 0;
        return result;
    };

    for (size_t i = 0; i < kByteSuffexes.size(); ++i)
    {
        if (toupper(str.back()) == toupper(kByteSuffexes[i]))
        {
            return subDouble(
                str.substr(0, str.size() - 1),
                std::pow(kByteSuffixLimit, static_cast<double>(i + 1)));
        }
    }

    // NOTE: avoid double to be the most precise
    std::size_t endPos = 0;
    const auto result = stoull(str, &endPos);
    if (isOk)
        *isOk = endPos > 0;
    return result;
}

uint64_t stringToBytes(const std::string_view& str, uint64_t defaultValue)
{
    bool isOk = false;
    uint64_t value = stringToBytes(str, &isOk);
    if (isOk)
        return value;
    else
        return defaultValue;
}

QString removeMnemonics(QString text)
{
    /**
     * Regular expression is:
     * (not match '&') match '&' (not match whitespace or '&')
     */
    return text.remove(QRegularExpression(QStringLiteral("(?<!&)&(?!([\\s&]|$))")));
}

QString escapeMnemonics(QString text)
{
    return text.replace("&","&&");
}

template <class T, class T2>
static QList<T> smartSplitInternal(
    const T& data,
    const T2 delimiter,
    const T2 quoteChar,
    bool keepEmptyParts)
{
    bool quoted = false;
    QList<T> rez;
    if (data.isEmpty())
        return rez;

    int lastPos = 0;
    for (int i = 0; i < data.size(); ++i)
    {
        if (data[i] == quoteChar)
            quoted = !quoted;
        else if (data[i] == delimiter && !quoted)
        {
            T value = data.mid(lastPos, i - lastPos);
            if (!value.isEmpty() || keepEmptyParts)
                rez << value;
            lastPos = i + 1;
        }
    }
    rez << data.mid(lastPos, data.size() - lastPos);

    return rez;
}

QList<QByteArray> smartSplit(
    const QByteArray& data,
    const char delimiter)
{
    return smartSplitInternal(data, delimiter, '\"', true);
}

QStringList smartSplit(
    const QString& data,
    const QChar delimiter,
    Qt::SplitBehavior splitBehavior)
{
    return smartSplitInternal(
        data,
        delimiter,
        QChar(L'\"'),
        splitBehavior == Qt::KeepEmptyParts);
}

QByteArray trimAndUnquote(const QByteArray& v)
{
    return trimAndUnquote(v, '\"');
}

QString trimAndUnquote(const QString& v)
{
    return trimAndUnquote(v, '\"');
}

static int doFormatJsonString(const char* srcPtr, const char* srcEnd, char* dstPtr)
{
    static const int INDENT_SIZE = 4; //< How many spaces to add to formatting.
    static const char INDENT_SYMBOL = ' '; //< Space filler.
    static QByteArray OUTPUT_DELIMITER("\n"); //< New line.
    static const QByteArray INPUT_DELIMITERS("[]{},"); //< Delimiters in the input string.
    static const int INDENTS[] = {1, -1, 1, -1, 0}; //< Indentation offset for INPUT_DELIMITERS.
    const char* dstPtrBase = dstPtr;
    int indent = 0;
    bool quoted = false;
    bool escaped = false;
    for (; srcPtr < srcEnd; ++srcPtr)
    {
        if (*srcPtr == '"' && !escaped)
            quoted = !quoted;

        escaped = *srcPtr == '\\' && !escaped;

        int symbolIdx = INPUT_DELIMITERS.indexOf(*srcPtr);
        bool isDelimBefore = symbolIdx >= 0 && INDENTS[symbolIdx] < 0;
        if (!dstPtrBase)
            dstPtr++;
        else if (!isDelimBefore)
            *dstPtr++ = *srcPtr;

        if (symbolIdx >= 0 && !quoted)
        {
            if (dstPtrBase)
                memcpy(dstPtr, OUTPUT_DELIMITER.data(), OUTPUT_DELIMITER.size());
            dstPtr += OUTPUT_DELIMITER.size();
            indent += INDENT_SIZE * INDENTS[symbolIdx];
            if (dstPtrBase)
                memset(dstPtr, INDENT_SYMBOL, indent);
            dstPtr += indent;
        }

        if (dstPtrBase && isDelimBefore)
            *dstPtr++ = *srcPtr;
    }
    return (int) (dstPtr - dstPtrBase);
}

QByteArray formatJsonString(const QByteArray& data)
{
    QByteArray result;
    result.resize(doFormatJsonString(data.data(), data.data() + data.size(), 0));
    doFormatJsonString(data.data(), data.data() + data.size(), result.data());
    return result;
}

nx::Buffer formatJsonString(const nx::Buffer& data)
{
    nx::Buffer result;
    result.resize(doFormatJsonString(data.data(), data.data() + data.size(), 0));
    doFormatJsonString(data.data(), data.data() + data.size(), result.data());
    return result;
}

int stricmp(const QByteArray& left, const QByteArray& right)
{
    return stricmp(
        std::string_view(left.data(), left.size()),
        std::string_view(right.data(), right.size()));
}

void truncateToNul(QString* s)
{
    const int length = std::char_traits<char16_t>::length((const char16_t*) s->data());
    s->resize(length);
}

} // namespace nx::utils
