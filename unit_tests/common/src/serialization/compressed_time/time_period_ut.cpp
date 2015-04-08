/**********************************************************
* 26 feb 2015
* r.vasilenko
***********************************************************/

#include <iostream>
#include <iomanip>

#include <gtest/gtest.h>

#include <QtCore/QUuid>
#include <QDateTime>
#include <utils/serialization/compressed_time_functions.h>
#include <utils/fusion/fusion_adaptor.h>
#include "recording/time_period_list.h"

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

QN_FUSION_ADAPT_STRUCT(QnTimePeriod, (startTimeMs)(durationMs))

TEST( TimePeriodCompressedSerializationTest, SerializeCompressedTimePeriods)
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

    // 2. signed stream test

    srcData.clear();
    dstData.clear();
    buffer.clear();
    
    currentTime = QDateTime::currentMSecsSinceEpoch();
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

