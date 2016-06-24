/**********************************************************
* Jul 30, 2015
* a.kolesnikov
***********************************************************/

#include <nx/utils/string.h>

#include <gtest/gtest.h>

#include <QtCore/QDateTime>


TEST( nx::utils::parseDateTime, general )
{
    const QString testDateStr( "2015-01-01T12:00:01" );
    static const qint64 USEC_PER_MS = 1000;

    const QDateTime testDate = QDateTime::fromString( testDateStr, Qt::ISODate );
    const qint64 testTimestamp = testDate.toMSecsSinceEpoch();
    const qint64 testTimestampUSec = testTimestamp * USEC_PER_MS;

    ASSERT_EQ( nx::utils::parseDateTime( testDateStr ), testTimestampUSec );
    ASSERT_EQ( nx::utils::parseDateTime( QString::number(testTimestamp) ), testTimestampUSec );
    ASSERT_EQ( nx::utils::parseDateTime( QString::number(testTimestampUSec) ), testTimestampUSec );
}
