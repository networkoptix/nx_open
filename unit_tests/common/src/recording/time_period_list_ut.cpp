#include <gtest/gtest.h>

#include <QtCore/QDateTime>
#include <QtCore/QDebug>

#include <recording/time_period_list.h>

/** Initial test. Check if empty helper is valid. */
TEST( QnTimePeriodsListTest, init )
{
    /* Two years of chunks. */
    qint64 totalLengthMs = 1000ll * 60 * 60 * 24 * 365 * 2;

    /* One minute each. */
    qint64 chunkLengthMs = 1000ll * 60;

    /* 5 seconds spacing. */
    qint64 chunkSpaceMs = 1000ll * 5;

    const int mergingListsCount = 10;

    QVector<QnTimePeriodList> lists;
    for (int i = 0; i < mergingListsCount; ++i)
        lists << QnTimePeriodList();

    QnTimePeriodList resultPeriods;

    qint64 start = 0;
    while (start < totalLengthMs) {
        for (int i = 0; i < mergingListsCount; ++i)
            lists[i].push_back(QnTimePeriod(start + i, chunkLengthMs));

        resultPeriods.push_back(QnTimePeriod(start, chunkLengthMs + mergingListsCount - 1));
        start += (chunkLengthMs + chunkSpaceMs);
    }

    qint64 timestamp1 = QDateTime::currentMSecsSinceEpoch();
    QnTimePeriodList merged = QnTimePeriodList::mergeTimePeriods(lists);
    qint64 timestamp2 = QDateTime::currentMSecsSinceEpoch();

    qDebug() << resultPeriods.size() << "periods *" << mergingListsCount << "lists were merged in" << (timestamp2 - timestamp1) << "ms";
    ASSERT_EQ(resultPeriods, merged);
}


