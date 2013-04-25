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
        number = lit("0");
        suffix = lit("B");
    } else {
        double absSize = std::abs(static_cast<double>(size));
        int power = static_cast<int>(std::log(absSize / prefixThreshold) / std::log(1000.0));
        int unit = qBound(static_cast<int>(minPrefix), power, qMin(static_cast<int>(maxPrefix), static_cast<int>(arraysize(metricSuffixes) - 1)));

        suffix = lit((useBinaryPrefixes ? binarySuffixes : metricSuffixes)[unit]);
        number = (size < 0 ? lit("-") : QString()) + QString::number(absSize / std::pow(useBinaryPrefixes ? 1024.0 : 1000.0, unit), 'f', precision);

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

void ExtractToken( QString & buffer, const QString & string, int & pos, bool & isNumber )
{
	buffer.clear();
	if ( string.isNull() || pos >= string.length() )
		return;

	isNumber = false;
	QChar curr = string[ pos ];
	if ( curr == L'-' || curr == L'+' || curr.isDigit() )
	{
		if ( curr == L'-' || curr == L'+' )
			INCBUF();

		if ( !curr.isNull() && curr.isDigit() )
		{
			isNumber = true;
			while ( curr.isDigit() )
				INCBUF();

			if ( curr == L'.' )
			{
				INCBUF();
				while ( curr.isDigit() )
					INCBUF();
			}

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
		}
	}

	if ( !isNumber )
	{
		while ( curr != L'-' && curr != L'+' && !curr.isDigit() && pos < string.length() )
			INCBUF();
	}
}

int naturalStringCompare( const QString & lhs, const QString & rhs, Qt::CaseSensitivity caseSensitive )
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
		ExtractToken( lhsBufferQStr, lhs, ii, lhsNumber );
		ExtractToken( rhsBufferQStr, rhs, jj, rhsNumber );

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
				if ( retVal )
				{
					QChar nextChar;
					if ( ii < lhs.length() ) // more on the lhs
						nextChar = lhs[ ii ];
					else if ( jj < rhs.length() ) // more on the rhs
						nextChar = rhs[ jj ];

					bool nextIsNum = ( nextChar == L'-' || nextChar == L'+' || nextChar.isDigit() );

					if ( nextIsNum )
						retVal = -1*retVal;
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
		return -1;
	else if ( jj < rhs.length() )
		return 1;
	else
		return 0;
}

bool naturalStringLessThan( const QString & lhs, const QString & rhs )
{
	return naturalStringCompare( lhs, rhs, Qt::CaseSensitive ) < 0;
}

bool naturalStringCaseInsensitiveLessThan( const QString & lhs, const QString & rhs )
{
	return naturalStringCompare( lhs, rhs, Qt::CaseInsensitive ) < 0;
}

QStringList naturalStringSort( const QStringList & list, Qt::CaseSensitivity caseSensitive )
{
	QStringList retVal = list;
	if ( caseSensitive == Qt::CaseSensitive )
		qSort( retVal.begin(), retVal.end(), naturalStringLessThan );
	else
		qSort( retVal.begin(), retVal.end(), naturalStringCaseInsensitiveLessThan );
	return retVal;
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
