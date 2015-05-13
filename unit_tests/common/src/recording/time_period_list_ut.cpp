#include <gtest/gtest.h>

#include <QtCore/QDateTime>
#include <QtCore/QDebug>

#include <recording/time_period_list.h>

//#define QN_NO_BIG_DATA_TEST

void PrintTo(const QnTimePeriod& period, ::std::ostream* os) {
    const QString fmt = "%1 - %2";
    QString result;
    if (!period.isInfinite())
        result = fmt.arg(period.startTimeMs).arg(period.endTimeMs());
    else
        result = fmt.arg(period.startTimeMs).arg("Inf");
    *os << result.toStdString();
}

void PrintTo(const QnTimePeriodList& periodList, ::std::ostream* os) {
    const QString fmt = "{%1 - %2} ";   
    for (const QnTimePeriod &period: periodList) {
        QString result;
        if (!period.isInfinite())
            result = fmt.arg(period.startTimeMs).arg(period.endTimeMs());
        else
            result = fmt.arg(period.startTimeMs).arg("Inf");
        *os << result.toStdString();
    }
}

TEST( QnTimePeriodsListTest, mergeBigData )
{
#ifdef QN_NO_BIG_DATA_TEST
    return;
#endif

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

TEST( QnTimePeriodsListTest, unionBigData )
{
#ifdef QN_NO_BIG_DATA_TEST
    return;
#endif

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
    for (int i = 1; i < mergingListsCount; ++i)
        QnTimePeriodList::unionTimePeriods(lists[0], lists[i]);

    qint64 timestamp2 = QDateTime::currentMSecsSinceEpoch();

    qDebug() << resultPeriods.size() << "periods *" << mergingListsCount << "lists were unioned in" << (timestamp2 - timestamp1) << "ms";
    ASSERT_EQ(resultPeriods, lists[0]);
}

TEST( QnTimePeriodsListTest, unionBySameChunk )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList appendingList;
    appendingList << QnTimePeriod(12, 1) << QnTimePeriod(30, 5);

    QnTimePeriodList resultList;
    resultList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList::unionTimePeriods(sourceList, appendingList);

    ASSERT_EQ(resultList, sourceList);
}

TEST( QnTimePeriodsListTest, unionByAppending )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList appendingList;
    appendingList << QnTimePeriod(12, 5) << QnTimePeriod(30, 7);

    QnTimePeriodList resultList;
    resultList << QnTimePeriod(10, 7) << QnTimePeriod(20, 5) << QnTimePeriod(30, 7) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList::unionTimePeriods(sourceList, appendingList);

    ASSERT_EQ(resultList, sourceList);
}

TEST( QnTimePeriodsListTest, unionByPrepending )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList appendingList;
    appendingList << QnTimePeriod(8, 5) << QnTimePeriod(28, 7);

    QnTimePeriodList resultList;
    resultList << QnTimePeriod(8, 7) << QnTimePeriod(20, 5) << QnTimePeriod(28, 7) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList::unionTimePeriods(sourceList, appendingList);

    ASSERT_EQ(resultList, sourceList);
}

TEST( QnTimePeriodsListTest, unionByExtending )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList appendingList;
    appendingList << QnTimePeriod(8, 9) << QnTimePeriod(18, 9);

    QnTimePeriodList resultList;
    resultList << QnTimePeriod(8, 9) << QnTimePeriod(18, 9) << QnTimePeriod(30, 5) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList::unionTimePeriods(sourceList, appendingList);

    ASSERT_EQ(resultList, sourceList);
}

TEST( QnTimePeriodsListTest, unionByCombining )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList appendingList;
    appendingList << QnTimePeriod(12, 10) << QnTimePeriod(25, 5);

    QnTimePeriodList resultList;
    resultList << QnTimePeriod(10, 25) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList mergedList = QnTimePeriodList::mergeTimePeriods(QVector<QnTimePeriodList>() << sourceList << appendingList);
    ASSERT_EQ(mergedList, resultList);

    QnTimePeriodList::unionTimePeriods(sourceList, appendingList);

    ASSERT_EQ(resultList, sourceList);
}

TEST( QnTimePeriodsListTest, unionByCombiningSeveral )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList appendingList;
    appendingList << QnTimePeriod(5, 30);

    QnTimePeriodList resultList = QnTimePeriodList::mergeTimePeriods(QVector<QnTimePeriodList>() << sourceList << appendingList);

    QnTimePeriodList::unionTimePeriods(sourceList, appendingList);

    ASSERT_EQ(resultList, sourceList);
}


TEST( QnTimePeriodsListTest, unionByLimits )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(20, 5);

    QnTimePeriodList appendingList;
    appendingList << QnTimePeriod(10, 5)  << QnTimePeriod(30, 5) ;

    QnTimePeriodList resultList = QnTimePeriodList::mergeTimePeriods(QVector<QnTimePeriodList>() << sourceList << appendingList);

    QnTimePeriodList::unionTimePeriods(sourceList, appendingList);

    ASSERT_EQ(resultList, sourceList);
}

TEST( QnTimePeriodsListTest, serializationUnsigned )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QByteArray serialized;
    sourceList.encode(serialized);

    QnTimePeriodList resultList;
    resultList.decode(serialized);

    ASSERT_EQ(resultList, sourceList);
}

TEST( QnTimePeriodsListTest, overwriteEmptyListTail )
{
    QnTimePeriodList sourceList;

    QnTimePeriodList tail;
    tail << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, 5);

    QnTimePeriodList::overwriteTail(sourceList, tail, 10);

    QnTimePeriodList resultList;
    resultList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, 5);

    ASSERT_EQ(resultList, sourceList);
}

TEST( QnTimePeriodsListTest, overwriteTailEmpty )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, 5);

    QnTimePeriodList tail;

    QnTimePeriodList::overwriteTail(sourceList, tail, 50);

    QnTimePeriodList resultList;
    resultList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, 5);

    ASSERT_EQ(resultList, sourceList);
}

TEST( QnTimePeriodsListTest, overwriteTailByPeriodStart )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList tail;
    tail << QnTimePeriod(40, 5) << QnTimePeriod(50, QnTimePeriod::infiniteDuration());

    QnTimePeriodList::overwriteTail(sourceList, tail, 40);

    QnTimePeriodList resultList;
    resultList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, 5) << QnTimePeriod(50, QnTimePeriod::infiniteDuration());

    ASSERT_EQ(resultList, sourceList);
}

TEST( QnTimePeriodsListTest, overwriteTailByTrimLive )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList tail;

    QnTimePeriodList::overwriteTail(sourceList, tail, 50);

    QnTimePeriodList resultList;
    resultList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, 10);

    ASSERT_EQ(resultList, sourceList);
}

TEST( QnTimePeriodsListTest, overwriteTailByTrimLiveAndAppend )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, QnTimePeriod::infiniteDuration());

    QnTimePeriodList tail;
    tail << QnTimePeriod(55, QnTimePeriod::infiniteDuration());

    QnTimePeriodList::overwriteTail(sourceList, tail, 50);

    QnTimePeriodList resultList;
    resultList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, 10) << QnTimePeriod(55, QnTimePeriod::infiniteDuration());;

    ASSERT_EQ(resultList, sourceList);
}

TEST( QnTimePeriodsListTest, overwriteTailByTrimAndAppend )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, 20);

    QnTimePeriodList tail;
    tail << QnTimePeriod(55, QnTimePeriod::infiniteDuration());

    QnTimePeriodList::overwriteTail(sourceList, tail, 50);

    QnTimePeriodList resultList;
    resultList << QnTimePeriod(10, 5) << QnTimePeriod(20, 5) << QnTimePeriod(30, 5) << QnTimePeriod(40, 10) << QnTimePeriod(55, QnTimePeriod::infiniteDuration());;

    ASSERT_EQ(resultList, sourceList);
}
