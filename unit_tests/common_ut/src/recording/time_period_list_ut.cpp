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
        sourceList << QnTimePeriod(146, 6);
        resultList << QnTimePeriod(146, 6);
        sourceList.excludeTimePeriods(excludeList);

        ASSERT_EQ(resultList, sourceList);
    }

    {
        QnTimePeriodList sourceList;
        sourceList
            << QnTimePeriod(1509017499000, 45000)
            << QnTimePeriod(1509028348000, 1419000)
            << QnTimePeriod(1509029767000, 11000)
            << QnTimePeriod(1509029785000, 1216000)
            << QnTimePeriod(1509031001000, 1387000)
            << QnTimePeriod(1509032388000, 1198000)
            << QnTimePeriod(1509033586000, 1486000)
            << QnTimePeriod(1509035072000, 1243000)
            << QnTimePeriod(1509036315000, 1198000)
            << QnTimePeriod(1509037513000, 109000);

        QnTimePeriodList excludeList;
        excludeList
            << QnTimePeriod(1509028349066, 8926)
            << QnTimePeriod(1509029767717, 9266)
            << QnTimePeriod(1509029785404, 9566)
            << QnTimePeriod(1509031001416, 9566)
            << QnTimePeriod(1509032388176, 9799)
            << QnTimePeriod(1509033586423, 9566)
            << QnTimePeriod(1509035072141, 9833)
            << QnTimePeriod(1509036315450, 9533);

        QnTimePeriodList resultList;
        resultList
            << QnTimePeriod(1509017499000, 45000)

            << QnTimePeriod(1509028348000, 1066)
            << QnTimePeriod(1509028357992, 1409008)

            << QnTimePeriod(1509029767000, 717)
            << QnTimePeriod(1509029776983, 1017)

            << QnTimePeriod(1509029785000, 404)
            << QnTimePeriod(1509029794970, 1206030)

            << QnTimePeriod(1509031001000, 416)
            << QnTimePeriod(1509031010982, 1377018)

            << QnTimePeriod(1509032388000, 176)
            << QnTimePeriod(1509032397975, 1188025)

            << QnTimePeriod(1509033586000, 423)
            << QnTimePeriod(1509033595989, 1476011)

            << QnTimePeriod(1509035072000, 141)
            << QnTimePeriod(1509035081974, 1233026)

            << QnTimePeriod(1509036315000, 450)
            << QnTimePeriod(1509036324983, 1188017)

            << QnTimePeriod(1509037513000, 109000);

        sourceList.excludeTimePeriods(excludeList);
        ASSERT_EQ(resultList, sourceList);
    }

    {
        QnTimePeriodList sourceList;
        QnTimePeriodList resultList;
        for (int i = 100; i < 200; i += 10)
        {
            sourceList << QnTimePeriod(i, 5);
            resultList << QnTimePeriod(i, 5);
        }

        QnTimePeriodList leftExcludeList;
        for (int i = 0; i < 100; i += 10)
            leftExcludeList << QnTimePeriod(i, 5);

        QnTimePeriodList rightExcludeList;
        for (int i = 200; i < 300; i += 10)
            rightExcludeList << QnTimePeriod(i, 5);

        sourceList.excludeTimePeriods(QnTimePeriodList());
        ASSERT_EQ(sourceList, resultList);

        sourceList.excludeTimePeriods(leftExcludeList);
        ASSERT_EQ(sourceList, resultList);

        sourceList.excludeTimePeriods(rightExcludeList);
        ASSERT_EQ(sourceList, resultList);
    }

    {
        QnTimePeriodList sourceList;
        QnTimePeriodList resultList;
        for (int i = 0; i < 200; i += 10)
        {
            sourceList << QnTimePeriod(i, 5);

            if (i < 50 || i > 150)
                resultList << QnTimePeriod(i, 5);
        }

        QnTimePeriodList excludeList;
        for (int i = 50; i <= 150; i += 10)
            excludeList << QnTimePeriod(i, 5);

        sourceList.excludeTimePeriods(excludeList);
        ASSERT_EQ(sourceList, resultList);
    }

    {
        QnTimePeriodList sourceList;
        QnTimePeriodList sourceList2;
        QnTimePeriodList resultList;
        for (int i = 0; i < 200; i += 10)
        {
            sourceList << QnTimePeriod(i, 5);
            sourceList2 << QnTimePeriod(i, 5);

            if (i < 150)
                resultList << QnTimePeriod(i, 5);
        }

        QnTimePeriodList excludeList;
        excludeList << QnTimePeriod(150, 150);

        QnTimePeriodList infiniteExcludeList;
        infiniteExcludeList << QnTimePeriod(150, -1);

        sourceList.excludeTimePeriods(infiniteExcludeList);
        ASSERT_EQ(sourceList, resultList);

        sourceList2.excludeTimePeriods(excludeList);
        ASSERT_EQ(sourceList2, resultList);
    }

    {
        QnTimePeriodList sourceList;
        QnTimePeriodList resultList;
        QnTimePeriodList excludeList;
        for (int i = 0; i < 200; i += 10)
        {
            if (i < 100)
            {
                sourceList << QnTimePeriod(i, 8);
                excludeList << QnTimePeriod(i + 1, 8);
                resultList << QnTimePeriod(i, 1);
            }
            else
            {
                excludeList << QnTimePeriod(i, 8);
            }
        }

        sourceList.excludeTimePeriods(excludeList);
        ASSERT_EQ(sourceList, resultList);
    }
}

void checkLists(const QnTimePeriodList& expectedList, const QnTimePeriodList& result)
{
    ASSERT_EQ(expectedList.size(), result.size());
    for (int i = 0; i < expectedList.size(); ++i)
    {
        ASSERT_EQ(expectedList[i].startTimeMs, result[i].startTimeMs);
        ASSERT_EQ(expectedList[i].durationMs, result[i].durationMs);
    }
}

TEST(QnTimePeriodsListTest, filterTimePeriodsAsc)
{
    QnTimePeriodList data;
    data << QnTimePeriod(15, 6)
        << QnTimePeriod(24, 2)
        << QnTimePeriod(32, 10)
        << QnTimePeriod(52, 20)
        << QnTimePeriod(85, 4)
        << QnTimePeriod(100, 5)
        << QnTimePeriod(110, 35);

    auto itr = std::lower_bound(data.begin(), data.end(), 24);
    auto endItr = std::lower_bound(data.begin(), data.end(), 52);

    auto result = data.filterTimePeriods(itr, endItr, 1, true, 2, Qt::SortOrder::AscendingOrder);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ(24, result[0].startTimeMs);
    ASSERT_EQ(32, result[1].startTimeMs);

    result = data.filterTimePeriods(itr, endItr, 1, true, 1, Qt::SortOrder::AscendingOrder);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(24, result[0].startTimeMs);

    itr = data.begin();
    endItr = data.end();

    result = data.filterTimePeriods(itr, endItr, /*detail level*/ 6, true, 100, Qt::SortOrder::AscendingOrder);
    {
        QnTimePeriodList expectedList;
        expectedList
            << QnTimePeriod(15, 11)
            << QnTimePeriod(32, 10)
            << QnTimePeriod(52, 20)
            << QnTimePeriod(85, 4)
            << QnTimePeriod(100, 45);
        checkLists(expectedList, result);
    }

    result = data.filterTimePeriods(itr, endItr, /*detail level*/ 6, false, 100, Qt::SortOrder::AscendingOrder);
    {
        QnTimePeriodList expectedList;
        expectedList
            << QnTimePeriod(15, 11)
            << QnTimePeriod(32, 10)
            << QnTimePeriod(52, 20)
            << QnTimePeriod(100, 45);
        checkLists(expectedList, result);
    }

    data.last().durationMs = QnTimePeriod::infiniteDuration();
    result = data.filterTimePeriods(itr, endItr, /*detail level*/ 6, true, 100, Qt::SortOrder::AscendingOrder);
    ASSERT_EQ(5, result.size());
    ASSERT_EQ(QnTimePeriod::infiniteDuration(), result.last().durationMs);
}

TEST(QnTimePeriodsListTest, filterTimePeriodsDesc)
{
    QnTimePeriodList data;
    data << QnTimePeriod(15, 6)
        << QnTimePeriod(24, 2)
        << QnTimePeriod(32, 10)
        << QnTimePeriod(52, 20)
        << QnTimePeriod(85, 4)
        << QnTimePeriod(100, 5)
        << QnTimePeriod(110, 35);

    auto itr = std::lower_bound(data.begin(), data.end(), 24);
    auto endItr = std::lower_bound(data.begin(), data.end(), 52);

#if 0
    auto result = data.filterTimePeriods(itr, endItr, 1, true, 2, Qt::SortOrder::DescendingOrder);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ(32, result[0].startTimeMs);
    ASSERT_EQ(24, result[1].startTimeMs);

    result = data.filterTimePeriods(itr, endItr, 1, true, 1, Qt::SortOrder::DescendingOrder);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(32, result[0].startTimeMs);
#endif
    itr = data.begin();
    endItr = data.end();

    auto result = data.filterTimePeriods(itr, endItr, /*detail level*/ 6, true, 100, Qt::SortOrder::DescendingOrder);
    {
        QnTimePeriodList expectedList;
        expectedList
            << QnTimePeriod(100, 45)
            << QnTimePeriod(85, 4)
            << QnTimePeriod(52, 20)
            << QnTimePeriod(32, 10)
            << QnTimePeriod(15, 11);
        checkLists(expectedList, result);
    }

    result = data.filterTimePeriods(itr, endItr, /*detail level*/ 6, false, 100, Qt::SortOrder::DescendingOrder);
    {
        QnTimePeriodList expectedList;
        expectedList
            << QnTimePeriod(100, 45)
            << QnTimePeriod(52, 20)
            << QnTimePeriod(32, 10)
            << QnTimePeriod(15, 11);
        checkLists(expectedList, result);
    }

    data.last().durationMs = QnTimePeriod::infiniteDuration();
    result = data.filterTimePeriods(itr, endItr, /*detail level*/ 6, true, 100, Qt::SortOrder::DescendingOrder);

    {
        QnTimePeriodList expectedList;
        expectedList
            << QnTimePeriod(100, QnTimePeriod::infiniteDuration())
            << QnTimePeriod(85, 4)
            << QnTimePeriod(52, 20)
            << QnTimePeriod(32, 10)
            << QnTimePeriod(15, 11);
        checkLists(expectedList, result);
    }

}
