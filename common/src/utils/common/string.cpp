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

QString formatFileSize(qint64 size, int precision, int prefixThreshold, Qn::MetricPrefix minPrefix, Qn::MetricPrefix maxPrefix, bool useBinaryPrefixes, const QString pattern) {
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

bool naturalStringLessThan(const QString & lhs, const QString & rhs, bool enableFloat)
{
    return naturalStringCompare( lhs, rhs, Qt::CaseSensitive, enableFloat ) < 0;
}

bool naturalStringCaseInsensitiveLessThan(const QString & lhs, const QString & rhs, bool enableFloat)
{
    return naturalStringCompare( lhs, rhs, Qt::CaseInsensitive, enableFloat ) < 0;
}

QStringList naturalStringSort( const QStringList & list, Qt::CaseSensitivity caseSensitive )
{
    QStringList retVal = list;
    if ( caseSensitive == Qt::CaseSensitive ) {
        qSort( retVal.begin(), retVal.end(), [](const QString &left, const QString &right) {
            return naturalStringLessThan(left, right);
        });
    } else {
        qSort( retVal.begin(), retVal.end(), [](const QString &left, const QString &right) {
            return naturalStringCaseInsensitiveLessThan(left, right);
        });
    }
    return retVal;
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
