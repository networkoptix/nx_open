#include <gtest/gtest.h>

#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>

#include <nx/utils/app_info.h>
#include <nx/utils/test_support/test_options.h>
#include <recording/time_period_list.h>

namespace {
#ifdef _DEBUG
    const qint64 bigDataTestsLimitMs = 5000;
#else
    const qint64 bigDataTestsLimitMs = 2000;
#endif
}

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

static const qint64 kTwoYearsMs = 1000ll * 60 * 60 * 24 * 365 * 2;
static const qint64 kTwoMonthMs = 1000ll * 60 * 60 * 24 * 60;
static const qint64 kChunkLengthMs = 1000ll * 60; //< One minute each.
static const qint64 kChunkSpaceMs = 1000ll * 5; //< 5 seconds spacing.
static const  int kMergingListsCount = 10;

TEST( QnTimePeriodsListTest, mergeBigData )
{
    std::vector<QnTimePeriodList> lists;
    for (int i = 0; i < kMergingListsCount; ++i)
        lists.push_back(QnTimePeriodList());

    QnTimePeriodList resultPeriods;

    const auto totalLengthMs = nx::utils::AppInfo::isArm() ? kTwoMonthMs : kTwoMonthMs;
    qint64 start = 0;
    while (start < totalLengthMs)
    {
        for (int i = 0; i < kMergingListsCount; ++i)
            lists[i].push_back(QnTimePeriod(start + i, kChunkLengthMs));

        resultPeriods.push_back(QnTimePeriod(start, kChunkLengthMs + kMergingListsCount - 1));
        start += (kChunkLengthMs + kChunkSpaceMs);
    }

    QElapsedTimer t;
    t.start();
    QnTimePeriodList merged = QnTimePeriodList::mergeTimePeriods(lists);
    qint64 elapsed =  t.elapsed();

    ASSERT_EQ(resultPeriods, merged);
    if (!nx::utils::AppInfo::isArm() && !nx::utils::TestOptions::areTimeAssertsDisabled())
        ASSERT_LE(elapsed, bigDataTestsLimitMs);
}

TEST( QnTimePeriodsListTest, unionBigData )
{
    QVector<QnTimePeriodList> lists;
    for (int i = 0; i < kMergingListsCount; ++i)
        lists << QnTimePeriodList();

    QnTimePeriodList resultPeriods;

    const auto totalLengthMs = nx::utils::AppInfo::isArm() ? kTwoMonthMs : kTwoMonthMs;
    qint64 start = 0;
    while (start < totalLengthMs)
    {
        for (int i = 0; i < kMergingListsCount; ++i)
            lists[i].push_back(QnTimePeriod(start + i, kChunkLengthMs));

        resultPeriods.push_back(QnTimePeriod(start, kChunkLengthMs + kMergingListsCount - 1));
        start += (kChunkLengthMs + kChunkSpaceMs);
    }

    QElapsedTimer t;
    t.start();
    for (int i = 1; i < kMergingListsCount; ++i)
        QnTimePeriodList::unionTimePeriods(lists[0], lists[i]);
    qint64 elapsed =  t.elapsed();

    ASSERT_EQ(resultPeriods, lists[0]);
    if (!nx::utils::AppInfo::isArm() && !nx::utils::TestOptions::areTimeAssertsDisabled())
        ASSERT_LE(elapsed, bigDataTestsLimitMs);
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

    std::vector<QnTimePeriodList> periods;
    periods.push_back(sourceList);
    periods.push_back(appendingList);
    QnTimePeriodList mergedList = QnTimePeriodList::mergeTimePeriods(periods);
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

    std::vector<QnTimePeriodList> periods;
    periods.push_back(sourceList);
    periods.push_back(appendingList);
    QnTimePeriodList resultList = QnTimePeriodList::mergeTimePeriods(periods);

    QnTimePeriodList::unionTimePeriods(sourceList, appendingList);

    ASSERT_EQ(resultList, sourceList);
}


TEST( QnTimePeriodsListTest, unionByLimits )
{
    QnTimePeriodList sourceList;
    sourceList << QnTimePeriod(20, 5);

    QnTimePeriodList appendingList;
    appendingList << QnTimePeriod(10, 5)  << QnTimePeriod(30, 5) ;

    std::vector<QnTimePeriodList> periods;
    periods.push_back(sourceList);
    periods.push_back(appendingList);
    QnTimePeriodList resultList = QnTimePeriodList::mergeTimePeriods(periods);

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

TEST( QnTimePeriodsListTest, includingPeriods )
{
    QnTimePeriodList sourceList;
    for (int i = 10; i < 100; i += 10)
        sourceList << QnTimePeriod(i, 5);
    sourceList << QnTimePeriod(10, 5) << QnTimePeriod(22, 10) << QnTimePeriod(45, 4) << QnTimePeriod(56, 4) << QnTimePeriod(69, 150 - 69);

    QnTimePeriodList testList;
    for (const QnTimePeriod &period: sourceList)
        testList.includeTimePeriod(period);

    QnTimePeriodList resultList;
    resultList << QnTimePeriod(10, 5) << QnTimePeriod(20, 15) << QnTimePeriod(40, 9) << QnTimePeriod(50, 5) << QnTimePeriod(56, 9) << QnTimePeriod(69, 150 - 69);

    ASSERT_EQ(resultList, testList);
}

TEST( QnTimePeriodsListTest, excludingPeriods )
{
    QnTimePeriodList sourceList;
    for (int i = 10; i < 150; i += 10)
        sourceList << QnTimePeriod(i, 5);

    QnTimePeriodList excludeList;
    excludeList << QnTimePeriod(15, 6) << QnTimePeriod(24, 2) << QnTimePeriod(32, 10) << QnTimePeriod(52, 20) << QnTimePeriod(85, 5) << QnTimePeriod(100, 5) << QnTimePeriod(110, 35);

    for (const QnTimePeriod &period: excludeList)
        sourceList.excludeTimePeriod(period);

    QnTimePeriodList resultList;
    resultList << QnTimePeriod(10, 5) << QnTimePeriod(21, 3) << QnTimePeriod(30, 2) << QnTimePeriod(42, 3) << QnTimePeriod(50, 2) << QnTimePeriod(72, 3) << QnTimePeriod(80, 5) << QnTimePeriod(90, 5);

    ASSERT_EQ(resultList, sourceList);
}

TEST(QnTimePeriodsListTest, excludePeriodList)
{
    QnTimePeriodList excludeList;
    excludeList << QnTimePeriod(15, 6) << QnTimePeriod(24, 2) << QnTimePeriod(32, 10) <<
        QnTimePeriod(52, 20) << QnTimePeriod(85, 5) << QnTimePeriod(100, 5) <<
        QnTimePeriod(110, 35);


    QnTimePeriodList resultList;
    resultList << QnTimePeriod(10, 5) << QnTimePeriod(21, 3) << QnTimePeriod(30, 2) << QnTimePeriod(42, 3) <<
        QnTimePeriod(50, 2) << QnTimePeriod(72, 3) << QnTimePeriod(80, 5) << QnTimePeriod(90, 5);

    {
        QnTimePeriodList sourceList;
        for (int i = 10; i < 150; i += 10)
            sourceList << QnTimePeriod(i, 5);
        sourceList.excludeTimePeriods(excludeList);

        ASSERT_EQ(resultList, sourceList);
    }

    {
        QnTimePeriodList sourceList;
        for (int i = 10; i < 150; i += 10)
            sourceList << QnTimePeriod(i, 5);
        sourceList << QnTimePeriod(144, 6);
        resultList << QnTimePeriod(145, 5);
        sourceList.excludeTimePeriods(excludeList);

        ASSERT_EQ(resultList, sourceList);
    }
}
