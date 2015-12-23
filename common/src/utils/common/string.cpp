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

qint64 parseDateTime( const QString& dateTimeStr )
{
    static const qint64 MIN_PER_HOUR = 60;
    static const qint64 SEC_PER_MIN = 60;
    static const qint64 HOUR_PER_DAY = 24;
    static const qint64 DAY_PER_NON_LEAP_YEAR = 365;
    static const qint64 DAY_PER_LEAP_YEAR = 366;
    static const qint64 MS_PER_SEC = 1000;
    static const qint64 USEC_PER_MS = 1000;

    if( dateTimeStr.toLower().trimmed() == lit( "now" ) )
    {
        return DATETIME_NOW;
    }
    else if( dateTimeStr.contains( L'T' ) || (dateTimeStr.contains( L'-' ) && !dateTimeStr.startsWith( L'-' )) )
    {
        const QStringList dateTimeParts = dateTimeStr.split( L'.' );
        QDateTime tmpDateTime = QDateTime::fromString( dateTimeParts[0], Qt::ISODate );
        if( dateTimeParts.size() > 1 )
            tmpDateTime = tmpDateTime.addMSecs( dateTimeParts[1].toInt() / 1000 );
        return tmpDateTime.toMSecsSinceEpoch() * USEC_PER_MS;
    }
    else
    {
        auto somethingSinceEpoch = dateTimeStr.toLongLong();
        //detecting millis or usec?
        if( somethingSinceEpoch > 0 && somethingSinceEpoch < (DAY_PER_NON_LEAP_YEAR * HOUR_PER_DAY * MIN_PER_HOUR * SEC_PER_MIN * MS_PER_SEC * USEC_PER_MS) )
            return somethingSinceEpoch * USEC_PER_MS;   //dateTime is in millis
        else
            return somethingSinceEpoch;
    }
}

qint64 parseDateTimeMsec( const QString& dateTimeStr )
{
    static const qint64 kUsecPerMs = 1000;

    qint64 usecSinceEpoch = parseDateTime(dateTimeStr);
    if (usecSinceEpoch < 0 || usecSinceEpoch == DATETIME_NOW)
        return usecSinceEpoch;  //special values are returned "as is"

    return usecSinceEpoch / kUsecPerMs;
}

QString formatFileSize(qint64 size, int precision, int prefixThreshold, Qn::MetricPrefix minPrefix, Qn::MetricPrefix maxPrefix, bool useBinaryPrefixes, const QString &pattern) {
    static const char *metricSuffixes[] = {"B", "kB",  "MB",  "GB",  "TB",  "PB",  "EB",  "ZB",  "YB"};
    static const char *binarySuffixes[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"};

    QString number, suffix;
    if (size == 0) {
        number = QLatin1String("0");
        suffix = QLatin1String("B");
    } else {
        double absSize = std::abs(static_cast<double>(size));
        int power = static_cast<int>(std::log(absSize / prefixThreshold) / std::log(1000.0));
        int unit = qBound(static_cast<int>(minPrefix), power, qMin(static_cast<int>(maxPrefix), static_cast<int>(arraysize(metricSuffixes) - 1)));

        suffix = QLatin1String((useBinaryPrefixes ? binarySuffixes : metricSuffixes)[unit]);
        number = (size < 0 ? QLatin1String("-") : QString()) + QString::number(absSize / std::pow(useBinaryPrefixes ? 1024.0 : 1000.0, unit), 'f', precision);

        /* Chop trailing zeros. */
        for(int i = number.size() - 1; ;i--) {
            QChar c = number[i];
            if(c == L'.') {
                number.chop(number.size() - i);
                break;
            }
            if(c != L'0') {
                number.chop(number.size() - i - 1);
                break;
            }
        }

    }

    return pattern.arg(number).arg(suffix);
}


QString xorEncrypt(const QString &plaintext, const QString &key) {
    if (key.isEmpty())
        return plaintext;

    QByteArray array(plaintext.toUtf8());
    QByteArray keyArray(key.toUtf8());

    for (int i = 0; i < array.size(); i++)
        array[i] = array[i] ^ keyArray[i % keyArray.size()];

    return QLatin1String(array.toBase64());
}

QString xorDecrypt(const QString &crypted, const QString &key) {
    if (key.isEmpty())
        return crypted;

    QByteArray array = QByteArray::fromBase64(crypted.toLatin1());
    QByteArray keyArray(key.toUtf8());

    for(int i = 0; i < array.size(); i++)
        array[i] = array[i] ^ keyArray[i % keyArray.size()];
    return QString::fromUtf8(array);
}


QString extractFileExtension(const QString &string) {
    int pos = string.lastIndexOf(L'.');
    if (pos < 0)
        return QString();

    QString result(L'.');
    while (++pos < string.length()) {
        QChar curr = string[pos];
        if (!curr.isLetterOrNumber())
            return result;
        result.append(curr);
    }

    return result;
}


QString generateUniqueString(const QStringList &usedStrings, const QString &defaultString, const QString &templateString) {
    QStringList lowerStrings;
    for (const QString &string: usedStrings)
        lowerStrings << string.toLower();

    QRegExp pattern = QRegExp(templateString.arg(lit("?([0-9]+)?")).toLower());

    /* Prepare new name. */
    int number = 0;
    for(const QString &string: lowerStrings) {
        if(!pattern.exactMatch(string))
            continue;

        number = qMax(number, pattern.cap(1).toInt());
    }

    if (number == 0) {
        if(defaultString.isEmpty()) {
            number = 1;
        } else if(!lowerStrings.contains(defaultString.toLower())) {
            return defaultString;
        } else {
            number = 2;
        }
    } else {
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

bool isNumberStart(const QChar &c) {
    /* We don't want to handle negative numbers as this leads to very strange
     * results. Think how "1-1" and "1-2" are going to be compared in this
     * case. */

    return
#if 0
        c == L'-' || c == L'+' ||
#endif
        c.isDigit();
}

void ExtractToken( QString & buffer, const QString & string, int & pos, bool & isNumber , bool enableFloat )
{
    buffer.clear();
    if ( string.isNull() || pos >= string.length() )
        return;

    isNumber = false;
    QChar curr = string[ pos ];
    // TODO:: Fix it
    // If you don't want to handle sign of the number, this isNumberStart is not needed indeed
    if ( isNumberStart(curr) )
    {
#if 0
        if ( curr == L'-' || curr == L'+' )
            INCBUF();
#endif

        if ( !curr.isNull() && curr.isDigit() )
        {
            isNumber = true;
            while ( curr.isDigit() )
                INCBUF();

            if ( curr == L'.' )
            {
                if(enableFloat) {
                    INCBUF();
                    while ( curr.isDigit() )
                        INCBUF();
                } else {
                    // We are done since we meet first character that is not expected
                    return;
                }
            }

            /* We don't want to handle exponential notation.
             * Besides, this implementation is buggy as it treats '14easd'
             * as a number. */
#if 0
            if ( !curr.isNull() && curr.toLower() == L'e' )
            {
                INCBUF();
                if ( curr == L'-' || curr == L'+' )
                    INCBUF();

                if ( curr.isNull() || !curr.isDigit() )
                    isNumber = false;
                else
                    while ( curr.isDigit() )
                        INCBUF();
            }
#endif
        }
    }

    if ( !isNumber )
    {
        while ( !isNumberStart(curr) && pos < string.length() )
            INCBUF();
    }
}

int naturalStringCompare( const QString & lhs, const QString & rhs, Qt::CaseSensitivity caseSensitive , bool enableFloat )
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

    while ( retVal == 0 && ii < lhs.length() && jj < rhs.length() )
    {
        ExtractToken( lhsBufferQStr, lhs, ii, lhsNumber , enableFloat );
        ExtractToken( rhsBufferQStr, rhs, jj, rhsNumber , enableFloat );

        if ( !lhsNumber && !rhsNumber )
        {
            // both strings curr val is a simple strcmp
            retVal = lhsBufferQStr.compare( rhsBufferQStr, caseSensitive );

            int maxLen = qMin( lhsBufferQStr.length(), rhsBufferQStr.length() );
            QString tmpRight = rhsBufferQStr.left( maxLen );
            QString tmpLeft = lhsBufferQStr.left( maxLen );
            if ( tmpLeft.compare( tmpRight, caseSensitive ) == 0 )
            {
                retVal = lhsBufferQStr.length() - rhsBufferQStr.length();

                if(retVal < 0) {
                    if(ii < lhs.length() && isNumberStart(lhs[ii]))
                        retVal *= -1;
                } else if(retVal > 0) {
                    if(jj < rhs.length() && isNumberStart(rhs[jj]))
                        retVal *= -1;
                }
            }
        }
        else if ( lhsNumber && rhsNumber )
        {
            // both numbers, convert and compare
            lhsValue = lhsBufferQStr.toDouble( &ok1 );
            rhsValue = rhsBufferQStr.toDouble( &ok2 );
            if ( !ok1 || !ok2 )
                retVal = lhsBufferQStr.compare( rhsBufferQStr, caseSensitive );
            else if ( lhsValue > rhsValue )
                retVal = 1;
            else if ( lhsValue < rhsValue )
                retVal = -1;
        }
        else
        {
            // completely arebitrary that a number comes before a string
            retVal = lhsNumber ? -1 : 1;
        }
    }

    if ( retVal != 0 )
        return retVal;
    if ( ii < lhs.length() )
        return 1;
    else if ( jj < rhs.length() )
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
        qSort(result.begin(), result.end(), [](const QString &left, const QString &right) {
            return naturalStringCompare(left, right, Qt::CaseSensitive) < 0;
        });
    else
        qSort(result.begin(), result.end(), naturalStringLess);
    return result;
}

void naturalStringCompareTestCase(const QString &l, const QString &r, int value) {
    assert(qBound(-1, naturalStringCompare(l, r, Qt::CaseInsensitive), 1) == value);
    assert(qBound(-1, naturalStringCompare(r, l, Qt::CaseInsensitive), 1) == -value);
}

void naturalStringCompareTest() {
    naturalStringCompareTestCase(lit("1-6.png"), lit("1-5.png"), 1);
    naturalStringCompareTestCase(lit("Layout"), lit("Layout 1"), -1);
    naturalStringCompareTestCase(lit("admin"), lit("admin1"), -1);
    naturalStringCompareTestCase(lit("20nov.nov"), lit("14exe_x64_read_only.exe"), 1);
}

void trimInPlace( QString* const str, const QString& symbols )
{
    int startPos = 0;
    for( ; startPos < str->size(); ++startPos )
    {
        if( !symbols.contains( str->at( startPos ) ) )
            break;
    }

    int endPos = str->size() - 1;
    for( ; endPos >= 0; --endPos )
    {
        if( !symbols.contains( str->at( endPos ) ) )
            break;
    }
    ++endPos;

    *str = str->mid( startPos, endPos > startPos ? (endPos - startPos) : 0 );
}

QString htmlBold(const QString &source) {
    return lit("<b>%1</b>").arg(source);
}

QString elideString(const QString &source, int maxLength, const QString &tail) {
    const auto tailLength = tail.length();
    return source.length() <= maxLength
        ? source
        : source.left(maxLength - tailLength) + tail;
}

QString htmlFormattedParagraph( const QString &text , int pixelSize , bool isBold /*= false */, bool isItalic /*= false*/ ) {
    static const auto kPTag = lit("<p style=\" text-ident: 0; font-size: %1px; font-weight: %2; font-style: %3; color: #FFF; margin-top: 0; margin-bottom: 0; margin-left: 0; margin-right: 0; \">%4</p>");

    if (text.isEmpty())
        return QString();

    const QString boldValue = (isBold ? lit("bold") : lit("normal"));
    const QString italicValue (isItalic ? lit("italic") : lit("normal"));

    static const auto kNewLineSymbol = L'\n';
    static const auto kNewLineTag = lit("<br>");

    const auto newFormattedText = text.trimmed().replace(kNewLineSymbol, kNewLineTag);
    return kPTag.arg(QString::number(pixelSize), boldValue, italicValue, newFormattedText);
}

QByteArray generateRandomName(int length)
{
    static const char kAlphaAndDigits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static const size_t kDigitsCount = 10;
    static_assert(kDigitsCount < sizeof(kAlphaAndDigits), "Check kAlphaAndDigits array");

    if (!length)
        return QByteArray();

    QByteArray str;
    str.resize(length);
    str[0] = kAlphaAndDigits[rand() % (sizeof(kAlphaAndDigits) / sizeof(*kAlphaAndDigits) - kDigitsCount - 1)];
    for (int i = 1; i < length; ++i)
        str[i] = kAlphaAndDigits[rand() % (sizeof(kAlphaAndDigits) / sizeof(*kAlphaAndDigits) - 1)];

    return str;
}
