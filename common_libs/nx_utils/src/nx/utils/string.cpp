#include "string.h"

#include <array>
#include <cmath>

#include <QtCore/QRegularExpression>

#include <nx/utils/log/assert.h>
#include <nx/utils/datetime.h>
#include <nx/utils/random.h>

namespace nx {
namespace utils {

QString formatFileSize(qint64 size, int precision, int prefixThreshold, MetricPrefix minPrefix, MetricPrefix maxPrefix, bool useBinaryPrefixes, const QString &pattern)
{
    static const std::array<QString, PrefixCount> metricSuffixes{{"B", "kB",  "MB",  "GB",  "TB",  "PB",  "EB",  "ZB",  "YB"}};
    static const std::array<QString, PrefixCount> binarySuffixes{{"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"}};

    QString number, suffix;
    if (size == 0)
    {
        number = "0";
        suffix = metricSuffixes[0];
    }
    else
    {
        double absSize = std::abs(static_cast<double>(size));
        int power = static_cast<int>(std::log(absSize / prefixThreshold) / std::log(1000.0));
        int unit = qBound(static_cast<int>(minPrefix), power, qMin(static_cast<int>(maxPrefix), PrefixCount - 1));

        suffix = (useBinaryPrefixes ? binarySuffixes : metricSuffixes)[unit];
        number = (size < 0 ? QLatin1String("-") : QString()) + QString::number(absSize / std::pow(useBinaryPrefixes ? 1024.0 : 1000.0, unit), 'f', precision);

        /* Chop trailing zeros. */
        for (int i = number.size() - 1; ; i--)
        {
            QChar c = number[i];
            if (c == L'.')
            {
                number.chop(number.size() - i);
                break;
            }
            if (c != L'0')
            {
                number.chop(number.size() - i - 1);
                break;
            }
        }

    }

    return pattern.arg(number).arg(suffix);
}

QString replaceCharacters(const QString &string, const char *symbols, const QChar &replacement)
{
    if (!symbols)
        return string;

    bool mask[256];
    memset(mask, 0, sizeof(mask));
    while (*symbols)
        mask[static_cast<int>(*symbols++)] = true; /* Static cast is here to silence GCC's -Wchar-subscripts. */

    QString result = string;
    for (int i = 0; i < result.size(); i++)
    {
        ushort c = result[i].unicode();
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

    if (dateTimeStr.toLower().trimmed() == lit("now"))
    {
        return DATETIME_NOW;
    }
    else if (dateTimeStr.contains(L'T') || (dateTimeStr.contains(L'-') && !dateTimeStr.startsWith(L'-')))
    {
        const QStringList dateTimeParts = dateTimeStr.split(L'.');
        QDateTime tmpDateTime = QDateTime::fromString(dateTimeParts[0], Qt::ISODate);
        if (dateTimeParts.size() > 1)
            tmpDateTime = tmpDateTime.addMSecs(dateTimeParts[1].toInt() / 1000);
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
    int pos = string.lastIndexOf(L'.');
    if (pos < 0)
        return QString();

    QString result(L'.');
    while (++pos < string.length())
    {
        QChar curr = string[pos];
        if (!curr.isLetterOrNumber())
            return result;
        result.append(curr);
    }

    return result;
}


QString generateUniqueString(const QStringList &usedStrings, const QString &defaultString, const QString &templateString)
{
    QStringList lowerStrings;
    for (const QString &string : usedStrings)
        lowerStrings << string.toLower();

    QRegExp pattern = QRegExp(templateString.arg(QLatin1String("?([0-9]+)?")).toLower());

    /* Prepare new name. */
    int number = 0;
    for (const QString &string : lowerStrings)
    {
        if (!pattern.exactMatch(string))
            continue;

        number = qMax(number, pattern.cap(1).toInt());
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

            if (curr == L'.')
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

QByteArray generateRandomName(int length)
{
    return random::generateName(length);
}

static const double kByteSuffixLimit = 1024;
static const std::vector<char> kByteSuffexes = { 'K', 'M', 'G', 'T' };

// TODO: #GDM #3.1 move out strings and logic to separate class
QString bytesToString(uint64_t bytes, int precision)
{
    double number = static_cast<double>(bytes);
    size_t suffix = 0;

    while (number >= kByteSuffixLimit
           && suffix <= kByteSuffexes.size())
    {
        number /= kByteSuffixLimit;
        ++suffix;
    }

    if (suffix == 0)
        return lm("%1").arg(number, 0, 'g', precision);

    return lm("%1%2").arg(number, 0, 'g', precision).arg(kByteSuffexes[suffix - 1]);
}

uint64_t stringToBytes(const QString& str, bool* isOk)
{
    const auto subDouble = [&](double multi)
    {
        return static_cast<uint64_t>(
            QStringRef(&str, 0, str.size() - 1).toDouble(isOk) * multi);
    };

    for (size_t i = 0; i < kByteSuffexes.size(); ++i)
    {
        if (str.endsWith(QChar::fromLatin1(kByteSuffexes[i]), Qt::CaseInsensitive))
            return subDouble(std::pow(kByteSuffixLimit, static_cast<double>(i + 1)));
    }

    // NOTE: avoid double to be the most precise
    return str.toULongLong(isOk);
}

uint64_t stringToBytes(const QString& str, uint64_t defaultValue)
{
    bool isOk = false;
    uint64_t value = stringToBytes(str, &isOk);
    if (isOk)
        return value;
    else
        return defaultValue;
}

uint64_t stringToBytesConst(const char* str)
{
    bool isOk = false;
    uint64_t value = stringToBytes(QString::fromUtf8(str), &isOk);
    NX_CRITICAL(isOk);
    return value;
}


std::vector<QnByteArrayConstRef> splitQuotedString(const QnByteArrayConstRef& src, char sep)
{
    std::vector<QnByteArrayConstRef> result;
    QnByteArrayConstRef::size_type curTokenStart = 0;
    bool quoted = false;
    for (QnByteArrayConstRef::size_type
        pos = 0;
        pos < src.size();
        ++pos)
    {
        const char ch = src[pos];
        if (!quoted && (ch == sep))
        {
            result.push_back(src.mid(curTokenStart, pos - curTokenStart));
            curTokenStart = pos + 1;
        }
        else if (ch == '"')
        {
            quoted = !quoted;
        }
    }
    result.push_back(src.mid(curTokenStart));

    return result;
}

QMap<QByteArray, QByteArray> parseNameValuePairs(
    const QnByteArrayConstRef& serializedData,
    char separator)
{
    QMap<QByteArray, QByteArray> nameValueContainer;
    parseNameValuePairs(
        serializedData,
        separator,
        &nameValueContainer);
    return nameValueContainer;
}

void parseNameValuePairs(
    const QnByteArrayConstRef& serializedData,
    char separator,
    QMap<QByteArray, QByteArray>* const params)
{
    const std::vector<QnByteArrayConstRef>& paramsList =
        splitQuotedString(serializedData, separator);
    for (const QnByteArrayConstRef& token : paramsList)
    {
        const auto& nameAndValue = splitQuotedString(token.trimmed(), '=');
        if (nameAndValue.empty())
            continue;
        QnByteArrayConstRef value = nameAndValue.size() > 1 ? nameAndValue[1] : QnByteArrayConstRef();
        params->insert(nameAndValue[0].trimmed(), value.trimmed("\""));
    }
}

QByteArray serializeNameValuePairs(
    const QMap<QByteArray, QByteArray>& params)
{
    QByteArray serializedData;
    serializeNameValuePairs(params, &serializedData);
    return serializedData;
}

void serializeNameValuePairs(
    const QMap<QByteArray, QByteArray>& params,
    QByteArray* const dstBuffer)
{
    for (QMap<QByteArray, QByteArray>::const_iterator
        it = params.begin();
        it != params.end();
        ++it)
    {
        if (it != params.begin())
            dstBuffer->append(", ");
        dstBuffer->append(it.key());
        dstBuffer->append("=\"");
        dstBuffer->append(it.value());
        dstBuffer->append("\"");
    }
}

QString removeMnemonics(QString text)
{
    /**
     * Regular expression is:
     * (not match '&') match '&' (not match whitespace or '&')
     */
    return text.remove(QRegularExpression(lit("(?<!&)&(?!([\\s&]|$))")));
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
    QString::SplitBehavior splitBehavior)
{
    return smartSplitInternal(
        data,
        delimiter,
        QChar(L'\"'),
        splitBehavior == QString::KeepEmptyParts);
}

template <class T, class T2>
static T unquoteStrInternal(const T& v, T2 quoteChar)
{
    T value = v.trimmed();
    int pos1 = value.startsWith(quoteChar) ? 1 : 0;
    int pos2 = value.endsWith(quoteChar) ? 1 : 0;
    return value.mid(pos1, value.length() - pos1 - pos2);
}

QByteArray unquoteStr(const QByteArray& v)
{
    return unquoteStrInternal(v, '\"');
}

QString unquoteStr(const QString& v)
{
    return unquoteStrInternal(v, L'\"');
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
    return dstPtr - dstPtrBase;
}

QByteArray formatJsonString(const QByteArray& data)
{
    QByteArray result;
    result.resize(doFormatJsonString(data.data(), data.data() + data.size(), 0));
    doFormatJsonString(data.data(), data.data() + data.size(), result.data());
    return result;
}

} // namespace utils
} // namespace nx
