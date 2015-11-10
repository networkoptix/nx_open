#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

#include <plugins/resource/server_archive/server_archive_delegate.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>
#include <recorder/storage_manager.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <core/resource_management/status_dictionary.h>
#include <common/common_module.h>
#include <core/resource_management/resource_properties.h>
#include <utils/common/util.h>

#include <functional>
#include <thread>
#include <chrono>
#include <list>
#include <vector>
#include <random>
#include <cmath>

const QString cameraFolderPrefix("camera");
const QString lqFolder("low_quality");
const QString hqFolder("hi_quality");

class TestHelper
{
    static const int DEFAULT_TIME_GAP_MS = 1000;
public:
    TestHelper(QStringList &&paths)
        : m_storageUrls(std::move(paths)),
          m_timeLine(DEFAULT_TIME_GAP_MS)
    {
        generateTestData();
    }
    
    ~TestHelper()
    {
        for (const auto &path : m_storageUrls)
            recursiveClean(path);
    }

    struct TimePeriod
    {
        int64_t startTimeMs;
        int durationMs;

        TimePeriod()
            : startTimeMs(-1),
              durationMs(-1)
        {}

        TimePeriod(int64_t stime, int duration) 
            : startTimeMs(stime),
              durationMs(duration)
        {}

        bool operator == (const TimePeriod &other) const 
        {
            return startTimeMs == other.startTimeMs && durationMs == other.durationMs;
        }

        static TimePeriod merge(const TimePeriod &tp1, const TimePeriod &tp2)
        {
            const TimePeriod &first = tp1.startTimeMs < tp2.startTimeMs ? tp1 : tp2;
            const TimePeriod &second = tp1 == first ? tp2 : tp1;

            // merge only overlapping periods
            assert(first.startTimeMs + first.durationMs > second.startTimeMs);
            TimePeriod ret;
            ret.startTimeMs = first.startTimeMs;
            ret.durationMs = 
                first.startTimeMs + first.durationMs > 
                second.startTimeMs + second.durationMs ? first.durationMs : 
                                                         second.startTimeMs - 
                                                         first.startTimeMs + 
                                                         second.durationMs;
            return ret;
        }
    };

    class TimeLine
    {
        typedef std::list<TimePeriod> TimePeriodList;
        typedef TimePeriodList::iterator TimePeriodListIterator;

    public:
        TimeLine(int timeGapMs) 
            : m_timePoint(std::numeric_limits<int64_t>::max()),
              m_timeGapMs(timeGapMs)
        {}

        void addTimePeriod(int64_t startTime, int duration)
        {
            addTimePeriod(TimePeriod(startTime, duration));
        }

        void addTimePeriod(const TimePeriod &period)
        {
            TimePeriod newPeriod = period;
            bool foundOverlapped = false;
            TimePeriodListIterator insertIterator = m_timeLine.end();
            bool firstToTheRight = true;

            for (auto it = m_timeLine.begin(); it != m_timeLine.end(); ++it) {
                if (newPeriod.startTimeMs + newPeriod.durationMs > it->startTimeMs && 
                    (newPeriod.startTimeMs <= it->startTimeMs || 
                     newPeriod.startTimeMs < it->startTimeMs + it->durationMs)) {
                    foundOverlapped = true;
                    newPeriod = TimePeriod::merge(newPeriod, *it);
                    if (newPeriod.startTimeMs < m_timePoint)
                        m_timePoint = newPeriod.startTimeMs;
                    it = m_timeLine.erase(it);
                    it = m_timeLine.insert(it, newPeriod);
                } else if (!foundOverlapped && firstToTheRight && 
                           it->startTimeMs > newPeriod.startTimeMs) {
                    firstToTheRight = false;
                    insertIterator = it;
                }
            }

            if (!foundOverlapped) {
               m_timeLine.insert(insertIterator, period); 
               if (period.startTimeMs < m_timePoint)
                   m_timePoint = period.startTimeMs;
            }
        }

        bool checkTime(int64_t time)
        {
            if (time < m_currentIt->startTimeMs || time > m_currentIt->durationMs +
                                                          m_currentIt->startTimeMs) {
                return false;
            }

            if (std::abs(time - m_timePoint) > m_timeGapMs)
                return false;

            m_timePoint = time;
            if (std::abs(m_timePoint - (m_currentIt->startTimeMs + 
                                        m_currentIt->durationMs)) < m_timeGapMs) {
                ++m_currentIt;
                if (m_currentIt != m_timeLine.end())
                    m_timePoint = m_currentIt->startTimeMs;
            }
            return true;
        }

        void finalize()
        {
            m_currentIt = m_timeLine.begin();
        }

    private:
        TimePeriodList m_timeLine;
        TimePeriodListIterator m_currentIt;
        int64_t m_timePoint;
        int m_timeGapMs;
    };

private:
    void generateTestData()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> normalStartDistMs(0, 2000);
        std::uniform_int_distribution<int> holeStartDistMs(5000, 20000);
        std::uniform_int_distribution<int> normalOrHoleDist(1, 5);
        std::uniform_int_distribution<int> durationDist(0, 1);

        for (int i = 0; i < m_storageUrls.size(); ++i) {
            QDir(closeDirPath(m_storageUrls[i]))
                    .mkpath(lit("%1_%2/%3").arg(cameraFolderPrefix)
                                           .arg(QString::number(i))
                                           .arg(lqFolder));
            QDir(closeDirPath(m_storageUrls[i]))
                    .mkpath(lit("%1_%2/%3").arg(cameraFolderPrefix)
                                           .arg(QString::number(i))
                                           .arg(hqFolder));
        }

        QFile testFile_1("://1445529661948_77158.mkv");
        QFile testFile_2("://1445529880231_64712.mkv");

        testFile_1.open(QIODevice::ReadOnly);
        testFile_2.open(QIODevice::ReadOnly);

        int64_t startTimeMs = 1445529661948;
        const int duration_1 = 77158;
        const int duration_2 = 64712;

        auto copyFile = [&](const QString& root, int cameraInd, const QString &quality)
        {
            int curDuration = durationDist(gen) == 0 ? duration_1 : duration_2;
            bool isHole = normalOrHoleDist(gen) == 1 ? true : false;
            int64_t curStartTime = isHole ? holeStartDistMs(gen) + startTimeMs : 
                                            normalStartDistMs(gen) + startTimeMs;
            QString fileName = lit("%1_%2.mkv").arg(startTimeMs).arg(curDuration);
            QString pathString = QnStorageManager::dateTimeStr(curStartTime, 
                                                               currentTimeZone()/60, 
                                                               lit("/"));
            pathString = lit("%1_%2/%3/%4").arg(cameraFolderPrefix)
                                           .arg(QString::number(cameraInd))
                                           .arg(quality)
                                           .arg(pathString);
            QDir(root).mkpath(pathString);
            if (curDuration == duration_1)
                testFile_1.copy(closeDirPath(root) + closeDirPath(pathString) + fileName);
            else
                testFile_2.copy(closeDirPath(root) + closeDirPath(pathString) + fileName);

            return curDuration;
        };

        for (int i = 0; i < 100; ++i) {
            //int timeDelta;
            for (int j = 0; j < m_storageUrls.size(); ++j) {
                int d1 = copyFile(m_storageUrls[j], j, lqFolder);
                int d2 = copyFile(m_storageUrls[j], j, hqFolder);
                //timeDelta = timeDelta > (d1 > d2 ? d1 : d2) ? timeDelta : 
                //                                              (d1 > d2 ? d1 : d2);
            }
            startTimeMs += duration_2;
        }
    }

    TimeLine &getTimeLine() {return m_timeLine;}

    bool recursiveClean(const QString &path) const
    {
        QDir dir(path);
        QFileInfoList entryList = dir.entryInfoList(
                QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot, 
                QDir::DirsFirst); 

        for (auto &entry : entryList) {
            auto tmp = entry.absoluteFilePath();
            if (entry.isDir()) {
                 if (!recursiveClean(entry.absoluteFilePath()))
                    return false;
            } 
            else if (entry.isFile()) {
                QFile f(entry.absoluteFilePath());
                f.setPermissions(QFile::ReadOther | QFile::WriteOther);
                auto result = f.remove();
                if (!result)
                    return false;
            }
        }
        dir.rmpath(path);
        return true;
    };
 private:
    QStringList m_storageUrls;
    TimeLine m_timeLine;
};

TEST(ServerArchiveDelegate_playback_test, TestHelper)
{
    TestHelper::TimePeriod tp1(0, 10);
    TestHelper::TimePeriod tp2(5, 30);

    auto tp3 = TestHelper::TimePeriod::merge(tp2, tp1);
    ASSERT_TRUE(tp3.startTimeMs == 0);
    ASSERT_TRUE(tp3.durationMs == 35);

    tp3 = TestHelper::TimePeriod::merge(tp1, tp2);
    ASSERT_TRUE(tp3.startTimeMs == 0);
    ASSERT_TRUE(tp3.durationMs == 35);
    
    tp1 = TestHelper::TimePeriod(5, 10);
    tp2 = TestHelper::TimePeriod(0, 20);

    tp3 = TestHelper::TimePeriod::merge(tp1, tp2);
    ASSERT_TRUE(tp3.startTimeMs == 0);
    ASSERT_TRUE(tp3.durationMs == 20);

    tp3 = TestHelper::TimePeriod::merge(tp2, tp1);
    ASSERT_TRUE(tp3.startTimeMs == 0);
    ASSERT_TRUE(tp3.durationMs == 20);

    TestHelper::TimeLine timeLine(2);
    timeLine.addTimePeriod(0, 10);
    timeLine.addTimePeriod(5, 15);
    timeLine.addTimePeriod(25, 10);
    timeLine.addTimePeriod(30, 20);
    timeLine.addTimePeriod(45, 15);
    timeLine.addTimePeriod(30, 20);

    timeLine.finalize();

    ASSERT_TRUE(timeLine.checkTime(0));
    ASSERT_TRUE(timeLine.checkTime(2));
    ASSERT_TRUE(timeLine.checkTime(4));
    ASSERT_TRUE(timeLine.checkTime(5));
    ASSERT_TRUE(timeLine.checkTime(6));
    ASSERT_TRUE(timeLine.checkTime(8));
    ASSERT_TRUE(timeLine.checkTime(10));
    ASSERT_TRUE(timeLine.checkTime(12));
    ASSERT_TRUE(timeLine.checkTime(14));
    ASSERT_TRUE(timeLine.checkTime(16));
    ASSERT_TRUE(timeLine.checkTime(18));
    ASSERT_TRUE(timeLine.checkTime(19));
    ASSERT_TRUE(!timeLine.checkTime(21));
    ASSERT_TRUE(!timeLine.checkTime(22));
    ASSERT_TRUE(!timeLine.checkTime(24));
    ASSERT_TRUE(timeLine.checkTime(25));
    ASSERT_TRUE(timeLine.checkTime(26));
    ASSERT_TRUE(timeLine.checkTime(28));
}

TEST(ServerArchiveDelegate_playback_test, Main) 
{
    const QString storageUrl_1 = qApp->applicationDirPath() + lit("/tmp/path1");
    const QString storageUrl_2 = qApp->applicationDirPath() + lit("/tmp/path2");

    std::unique_ptr<QnCommonModule> commonModule;
    if (!qnCommon) {
        commonModule = std::unique_ptr<QnCommonModule>(new QnCommonModule);
    }
    commonModule->setModuleGUID("{A680980C-70D1-4545-A5E5-72D89E33648B}");

    std::unique_ptr<QnStorageManager> normalStorageManager;
    if (!qnNormalStorageMan) {
        normalStorageManager = std::unique_ptr<QnStorageManager>(
                new QnStorageManager(QnServer::StoragePool::Normal));
    }
    normalStorageManager->stopAsyncTasks();

    std::unique_ptr<QnStorageManager> backupStorageManager;
    if (!qnBackupStorageMan) {
        backupStorageManager = std::unique_ptr<QnStorageManager>(
                new QnStorageManager(QnServer::StoragePool::Backup));
    }
    backupStorageManager->stopAsyncTasks();

    std::unique_ptr<QnResourceStatusDictionary> statusDictionary;
    if (!qnStatusDictionary) {
        statusDictionary = std::unique_ptr<QnResourceStatusDictionary>(
                new QnResourceStatusDictionary);
    }

    std::unique_ptr<QnResourcePropertyDictionary> propDictionary;
    if (!propertyDictionary) {
        propDictionary = std::unique_ptr<QnResourcePropertyDictionary>(
                new QnResourcePropertyDictionary);
    }

    std::unique_ptr<QnStorageDbPool> dbPool;
    if (!qnStorageDbPool) {
        dbPool = std::unique_ptr<QnStorageDbPool>(new QnStorageDbPool);
    }

    QnStorageResourcePtr storage_1 = QnStorageResourcePtr(new QnFileStorageResource);
    storage_1->setUrl(storageUrl_1);
    storage_1->setId("{04F021EA-9B0C-4ACD-923C-DE70B38D7021}");
    storage_1->setUsedForWriting(true);

    QnStorageResourcePtr storage_2 = QnStorageResourcePtr(new QnFileStorageResource);
    storage_2->setUrl(storageUrl_2);
    storage_2->setId("{FB08E83E-6BC2-48E2-9A09-4FDAA7065A46}");
    storage_2->setUsedForWriting(true);

    qnNormalStorageMan->addStorage(storage_1);
    qnBackupStorageMan->addStorage(storage_2);

    storage_1->setStatus(Qn::Online, true);
    storage_2->setStatus(Qn::Online, true);

    TestHelper testHelper(std::move(QStringList() << storageUrl_1 << storageUrl_2));
    //storage1->setStatus(Qn::Online, true);
    //storage2->setStatus(Qn::Online, true);
    //storage3->setStatus(Qn::Online, true);
}