/**********************************************************
* 26 feb 2015
* r.vasilenko
***********************************************************/

#include <iostream>
#include <iomanip>

#include <gtest/gtest.h>

#include <QtCore/QUuid>
#include <QDateTime>
#include <nx/fusion/serialization/compressed_time_functions.h>
#include <nx/fusion/fusion/fusion_adaptor.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>

class TimePeriodCompressedSerializationTest
    :
    public ::testing::Test
{
protected:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }
};


TEST( TimePeriodCompressedSerializationTest, SerializeCompressedTimePeriodsUnsigned)
{

    std::vector<QnTimePeriod> srcData;
    quint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    srcData.push_back(QnTimePeriod(currentTime, 30 * 1000ll));
    currentTime += 40 * 1000ll;
    srcData.push_back(QnTimePeriod(currentTime, 120 * 1000ll));
    currentTime += 1000 * 1000ll;
    srcData.push_back(QnTimePeriod(currentTime, 1000000ll));
    currentTime += 1000000ll;
    srcData.push_back(QnTimePeriod(3000000000ll * 1000ll, 5));

    QByteArray buffer;
    std::vector<QnTimePeriod> dstData;

    // 1. unsigned stream test
    {
        QnCompressedTimeWriter<QByteArray> inputStream(&buffer, false);
        QnCompressedTime::serialize( srcData, &inputStream);
    }

    {
        QnCompressedTimeReader<QByteArray> outputStream(&buffer);
        QnCompressedTime::deserialize( &outputStream, &dstData);
    }
    
    ASSERT_TRUE( dstData == srcData );
}


// 2. signed stream test
TEST( TimePeriodCompressedSerializationTest, SerializeCompressedTimePeriodsSigned)
{
    std::vector<QnTimePeriod> srcData;
    std::vector<QnTimePeriod> dstData;
    QByteArray buffer;
    
    quint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    srcData.push_back(QnTimePeriod(currentTime, 30 * 1000ll));
    currentTime += 20 * 1000ll;
    srcData.push_back(QnTimePeriod(currentTime, 20 * 1000ll));
    currentTime += 1000 * 1000ll;
    srcData.push_back(QnTimePeriod(currentTime, 1000000ll));
    srcData.push_back(QnTimePeriod(currentTime, 1000000ll));
    srcData.push_back(QnTimePeriod(3000000000ll * 1000ll, 5));

    {
        QnCompressedTimeWriter<QByteArray> inputStream(&buffer, true);
        QnCompressedTime::serialize( srcData, &inputStream);
    }

    {
        QnCompressedTimeReader<QByteArray> outputStream(&buffer);
        QnCompressedTime::deserialize( &outputStream, &dstData);
    }

    ASSERT_TRUE( dstData == srcData );

}

TEST( TimePeriodCompressedSerializationTest, SerializeCompressedMultiServerPeriodData)
{
    MultiServerPeriodDataList srcData;

    MultiServerPeriodData data;

    quint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    data.periods.push_back(QnTimePeriod(currentTime, 30 * 1000ll));
    currentTime += 40 * 1000ll;
    data.periods.push_back(QnTimePeriod(currentTime, 120 * 1000ll));
    currentTime += 1000 * 1000ll;
    data.periods.push_back(QnTimePeriod(currentTime, 1000000ll));
    currentTime += 1000000ll;
    data.periods.push_back(QnTimePeriod(3000000000ll * 1000ll, 5));       

    srcData.push_back(std::move(data));

    QByteArray buffer = QnCompressedTime::serialized(srcData, false);

    bool success = false;
    MultiServerPeriodDataList dstData = QnCompressedTime::deserialized(buffer, MultiServerPeriodDataList(), &success);

    ASSERT_TRUE( success ); 
    ASSERT_TRUE( dstData == srcData );
}

TEST( TimePeriodCompressedSerializationTest, SerializeCompressedMultiServerPeriodDataListUnsigned)
{
    MultiServerPeriodDataList srcData;

    for (int i = 0; i < 5; ++i) {
        MultiServerPeriodData data;

        quint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        data.periods.push_back(QnTimePeriod(currentTime, 30 * 1000ll));
        currentTime += 40 * 1000ll;
        data.periods.push_back(QnTimePeriod(currentTime, 120 * 1000ll));
        currentTime += 1000 * 1000ll;
        data.periods.push_back(QnTimePeriod(currentTime, 1000000ll));
        currentTime += 1000000ll;
        data.periods.push_back(QnTimePeriod(3000000000ll * 1000ll, 5));       

        srcData.push_back(std::move(data));
    }


    QByteArray buffer = QnCompressedTime::serialized(srcData, false);
    
    bool success = false;
    MultiServerPeriodDataList dstData = QnCompressedTime::deserialized(buffer, MultiServerPeriodDataList(), &success);

    ASSERT_TRUE( success ); 
    ASSERT_TRUE( dstData == srcData );
}

TEST( TimePeriodCompressedSerializationTest, SerializeCompressedMultiServerPeriodDataListSignedMinimal)
{
    MultiServerPeriodDataList srcData;
    MultiServerPeriodData data;

    data.periods.push_back(QnTimePeriod(1429891331259, 1));
    data.periods.push_back(QnTimePeriod(1433773026421, 1));
    srcData.push_back(std::move(data));

    QByteArray buffer = QnCompressedTime::serialized(srcData, true);

    bool success = false;
    MultiServerPeriodDataList dstData = QnCompressedTime::deserialized(buffer, MultiServerPeriodDataList(), &success);

    ASSERT_TRUE( success ); 
    ASSERT_TRUE( dstData == srcData );
}

TEST( TimePeriodCompressedSerializationTest, SerializeCompressedMultiServerPeriodDataListSigned)
{
    MultiServerPeriodDataList srcData;

    MultiServerPeriodData data;
    data.guid = QnUuid::createUuid();

    data.periods.push_back(QnTimePeriod(1429889084026, 1353727));
    data.periods.push_back(QnTimePeriod(1429891331259, 864863));
    data.periods.push_back(QnTimePeriod(1433773026421, 12088254));
    data.periods.push_back(QnTimePeriod(1433841105581, 13334769));
    data.periods.push_back(QnTimePeriod(1434386547349, 5359406));
    data.periods.push_back(QnTimePeriod(1434456216257, 587665));
    srcData.push_back(std::move(data));

    QByteArray buffer = QnCompressedTime::serialized(srcData, true);

    bool success = false;
    MultiServerPeriodDataList dstData = QnCompressedTime::deserialized(buffer, MultiServerPeriodDataList(), &success);

    ASSERT_TRUE( success ); 
    ASSERT_TRUE( dstData == srcData );
}
