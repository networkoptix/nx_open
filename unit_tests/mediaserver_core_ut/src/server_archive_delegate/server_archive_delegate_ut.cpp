#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

extern "C" {
#include <ffmpeg/libavformat/avformat.h>
}
#include <plugins/resource/server_archive/server_archive_delegate.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>
#include <recorder/storage_manager.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <core/resource_management/status_dictionary.h>
#include <common/common_module.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/network_resource.h>
#include <utils/common/util.h>

#ifndef _WIN32
#   include <platform/monitoring/global_monitor.h>
#   include <platform/platform_abstraction.h>
#endif

#include <functional>
#include <thread>
#include <chrono>
#include <list>
#include <vector>
#include <random>
#include <cmath>

const QString cameraFolder("camera");
const QString lqFolder("low_quality");
const QString hqFolder("hi_quality");

bool recursiveClean(const QString &path)
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

class TestHelper
{
   static const int DEFAULT_TIME_GAP_MS = 15001;

public:
    TestHelper(QStringList &&paths, int fileCount)
        : m_storageUrls(std::move(paths)),
          m_timeLine(DEFAULT_TIME_GAP_MS),
          m_fileCount(fileCount)
    {
        generateTestData();
        createStorages();
        loadMedia();
    }

    ~TestHelper() {}

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
            NX_ASSERT(first.startTimeMs + first.durationMs > second.startTimeMs);
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
        friend class TestHelper;

    public:
        TimeLine(int timeGapMs)
            : m_timePoint(std::numeric_limits<int64_t>::max()),
              m_timeGapMs(timeGapMs)
        {}

        void addTimePeriod(int64_t startTime, int duration)
        {
            addTimePeriod(TimePeriod(startTime, duration));
        }

        void setTimeGapMs(int timeGap)
        {
            m_timeGapMs = timeGap;
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
                    if (foundOverlapped) {
                        auto copyIt = it;
                        --copyIt;
                        m_timeLine.erase(copyIt);
                    }
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

            m_permTimePoint = m_timePoint;
        }

        bool checkTime(int64_t time)
        {
            if (m_currentIt == m_timeLine.cend())
                return true;

            if (time < m_timePoint)
                return false;

            //qDebug() << "current time period: ("
            //         << m_currentIt->startTimeMs << " " << m_currentIt->durationMs
            //         << lit("%1 left to the end ) ")
            //                .arg(m_currentIt->startTimeMs +
            //                     m_currentIt->durationMs -
            //                     m_timePoint)
            //         << "time: " << time << " m_time: " << m_timePoint
            //         << " diff: " << std::abs(time - m_timePoint);

            if (std::abs(time - m_timePoint) > m_timeGapMs) {
                if (time > m_timePoint && m_currentIt->startTimeMs +
                                          m_currentIt->durationMs -
                                          m_timePoint < m_timeGapMs*2) {
                    // maybe next time period will do
                    ++m_currentIt;
                    if (m_currentIt == m_timeLine.cend())
                        return true;
                    m_timePoint = m_currentIt->startTimeMs;
                    if (std::abs(time - m_timePoint) > m_timeGapMs)
                        return false;
                } else
                    return false;
            }

            m_timePoint = time;
            if (m_currentIt->startTimeMs + m_currentIt->durationMs <= m_timePoint)
                ++m_currentIt;
            return true;
        }

        void reset()
        {
            m_currentIt = m_timeLine.begin();
            m_timePoint = m_permTimePoint;
        }

    private:
        TimePeriodList          m_timeLine;
        TimePeriodListIterator  m_currentIt;
        int64_t                 m_timePoint;
        int64_t                 m_permTimePoint;
        int                     m_timeGapMs;
    };

public:
    TimeLine &getTimeLine() {return m_timeLine;}

    void print() const
    {
        qDebug() << lit("We have %1 files, %2 time periods")
                        .arg(m_fileCount)
                        .arg(m_timeLine.m_timeLine.size());

        qint64 prevStartTime;
        int prevDuration;
        qDebug() << "Time periods details: ";

        for (auto it = m_timeLine.m_timeLine.cbegin();
             it != m_timeLine.m_timeLine.cend();
             ++it) {
            qDebug() << it->startTimeMs << " " << it->durationMs;
            if (it != m_timeLine.m_timeLine.cbegin()) {
                qDebug() << "\tGap from previous: "
                         << it->startTimeMs - (prevStartTime + prevDuration) << "ms ("
                         << (it->startTimeMs - (prevStartTime + prevDuration))/1000
                         << "s )";
            }
            prevStartTime = it->startTimeMs;
            prevDuration = it->durationMs;
        }
    }

private:
    void generateTestData()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> normalStartDistMs(5500, 7000);
        std::uniform_int_distribution<int> holeStartDistMs(20000, 55000);
        std::uniform_int_distribution<int> normalOrHoleDist(1, 5);
        std::uniform_int_distribution<int> durationDist(0, 1);

        for (int i = 0; i < m_storageUrls.size(); ++i) {
            QDir(closeDirPath(m_storageUrls[i]))
                    .mkpath(lit("%2/%1").arg(cameraFolder).arg(lqFolder));
            QDir(closeDirPath(m_storageUrls[i]))
                    .mkpath(lit("%2/%1").arg(cameraFolder).arg(hqFolder));
        }

        QFile testFile_1("://1445529661948_77158.mkv");
        QFile testFile_2("://1445529880231_64712.mkv");

        int64_t startTimeMs = 1445529661948;
        const int duration_1 = 77158;
        const int duration_2 = 64712;

        auto writeHeaderTimestamp = [](const QString &fileName, int64_t timestamp)
        {
            QFile file(fileName);
            file.setPermissions(QFile::ReadOther | QFile::WriteOther);
            file.open(QIODevice::ReadWrite);
            if (!file.isOpen()) {
                qDebug() << "Write header %1 failed";
                return;
            }
            file.seek(0x219);
            auto buf = file.read(11);
            if (memcmp(buf.constData(), "START_TIMED", 11) != 0) {
                qDebug() << "Write header wrong offset %1";
                return;
            }
            file.seek(0x219 + 0xd);
            file.write(QString::number(timestamp).toLatin1().constData());
            file.close();
        };

        auto copyFile = [&](const QString& root, const QString &quality)
        {
            int curDuration = durationDist(gen) == 0 ? duration_1 : duration_2;
            bool isHole = normalOrHoleDist(gen) <= 3 ? true : false;
            int64_t curStartTime = isHole ? holeStartDistMs(gen) + startTimeMs :
                                            normalStartDistMs(gen) + startTimeMs;
            QString fileName = lit("%1_%2.mkv").arg(curStartTime).arg(curDuration);
            QString pathString = QnStorageManager::dateTimeStr(curStartTime,
                                                               currentTimeZone()/60,
                                                               lit("/"));
            pathString = lit("%2/%1/%3").arg(cameraFolder).arg(quality).arg(pathString);
            QDir(root).mkpath(pathString);
            QString fullFileName = closeDirPath(root) + closeDirPath(pathString)
                                                      + fileName;
            if (curDuration == duration_1)
                testFile_1.copy(fullFileName);
            else
                testFile_2.copy(fullFileName);

            writeHeaderTimestamp(fullFileName, curStartTime);
            m_timeLine.addTimePeriod(curStartTime, curDuration);
            return curStartTime + curDuration;
        };

        for (int i = 0; i < m_fileCount; ++i) {
            int64_t newTime = 0;
            for (int j = 0; j < m_storageUrls.size(); ++j) {
                int64_t d1 = copyFile(m_storageUrls[j], lqFolder);
                int64_t d2 = copyFile(m_storageUrls[j], hqFolder);
                newTime = newTime > (d1 > d2 ? d1 : d2) ? newTime : (d1 > d2 ? d1 : d2);
            }
            startTimeMs = newTime;
        }
    }

    void createStorages()
    {
        for (int i = 0; i < m_storageUrls.size(); ++i) {
            QnStorageResourcePtr storage = QnStorageResourcePtr(new QnFileStorageResource);
            storage->setUrl(m_storageUrls[i]);
            storage->setId(QnUuid::createUuid());
            storage->setUsedForWriting(true);
            if (i % 2 == 0)
                qnNormalStorageMan->addStorage(storage);
            else
                qnBackupStorageMan->addStorage(storage);
            storage->setStatus(Qn::Online, true);
            m_storages.push_back(storage);
        }
    }

    void loadMedia()
    {
        for (int i = 0; i < m_storageUrls.size(); ++i) {
            QnStorageManager *manager = i % 2 == 0 ? qnNormalStorageMan :
                                                     qnBackupStorageMan;
            manager->getFileCatalog(lit("%1").arg(cameraFolder),
                                    QnServer::LowQualityCatalog);

            manager->getFileCatalog(lit("%1").arg(cameraFolder),
                                    QnServer::HiQualityCatalog);
            manager->m_rebuildCancelled = false;

            manager->loadFullFileCatalogFromMedia(m_storages[i],
                                                  QnServer::LowQualityCatalog);

            manager->loadFullFileCatalogFromMedia(m_storages[i],
                                                  QnServer::HiQualityCatalog);
        }
    }

 private:
    QStringList m_storageUrls;
    TimeLine m_timeLine;
    int m_fileCount;
    std::vector<QnStorageResourcePtr> m_storages;
};

TEST(ServerArchiveDelegate_playback_test, Clean_before)
{
    recursiveClean(qApp->applicationDirPath() + lit("/tmp"));
}

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
    timeLine.addTimePeriod(0, 10);
    timeLine.addTimePeriod(0, 13);
    timeLine.addTimePeriod(5, 8);
    timeLine.addTimePeriod(5, 15);
    timeLine.addTimePeriod(25, 10);
    timeLine.addTimePeriod(30, 20);
    timeLine.addTimePeriod(45, 15);
    timeLine.addTimePeriod(30, 20);

    timeLine.reset();

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
    //ASSERT_TRUE(!timeLine.checkTime(21));
    //ASSERT_TRUE(!timeLine.checkTime(22));
    //ASSERT_TRUE(!timeLine.checkTime(24));
    ASSERT_TRUE(timeLine.checkTime(25));
    ASSERT_TRUE(timeLine.checkTime(26));
    ASSERT_TRUE(timeLine.checkTime(28));
}

static int lockmgr(void **mtx, enum AVLockOp op)
{
    QnMutex** qMutex = (QnMutex**) mtx;
    switch(op) {
        case AV_LOCK_CREATE:
            *qMutex = new QnMutex();
            return 0;
        case AV_LOCK_OBTAIN:
            (*qMutex)->lock();
            return 0;
        case AV_LOCK_RELEASE:
            (*qMutex)->unlock();
            return 0;
        case AV_LOCK_DESTROY:
            delete *qMutex;
            return 0;
    }
    return 1;
}

static void ffmpegInit()
{
    avcodec_register_all();

    if(av_lockmgr_register(lockmgr) != 0)
    {
        qCritical() << "Failed to register ffmpeg lock manager";
    }
}

TEST(ServerArchiveDelegate_playback_test, Main)
{
    const QString storageUrl_1 = qApp->applicationDirPath() + lit("/tmp/path1");
    const QString storageUrl_2 = qApp->applicationDirPath() + lit("/tmp/path2");

    std::unique_ptr<QnCommonModule> commonModule;
    if (!qnCommon) {
        commonModule = std::unique_ptr<QnCommonModule>(new QnCommonModule);
    }
    commonModule->setModuleGUID(QnUuid("{A680980C-70D1-4545-A5E5-72D89E33648B}"));

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

#ifndef _WIN32
    std::unique_ptr<QnPlatformAbstraction> platformAbstraction;
    if (!qnPlatform) {
        platformAbstraction = std::unique_ptr<QnPlatformAbstraction>(
            new QnPlatformAbstraction
        );
    }
#endif

    TestHelper testHelper(std::move(QStringList() << storageUrl_1 << storageUrl_2), 200);
    testHelper.print();

    QnNetworkResourcePtr cameraResource = QnNetworkResourcePtr(new QnNetworkResource);
    cameraResource->setPhysicalId(cameraFolder);

    ffmpegInit();

    QnServerArchiveDelegate archiveDelegate;
    archiveDelegate.open(cameraResource);
    archiveDelegate.setQuality(MEDIA_Quality_High, true);
    archiveDelegate.seek(0, true);

    testHelper.getTimeLine().reset();

    QnAbstractMediaDataPtr data;
    while(1) {
        data = archiveDelegate.getNextData();
        if (data)
            ASSERT_TRUE(testHelper.getTimeLine().checkTime(data->timestamp/1000));
        else
            break;
    }

    archiveDelegate.setQuality(MEDIA_Quality_Low, true);
    archiveDelegate.seek(0, true);
    testHelper.getTimeLine().reset();

    while(1) {
        data = archiveDelegate.getNextData();
        if (data)
            ASSERT_TRUE(testHelper.getTimeLine().checkTime(data->timestamp/1000));
        else
            break;
    }
}

TEST(ServerArchiveDelegate_playback_test, Clean_after)
{
    recursiveClean(qApp->applicationDirPath() + lit("/tmp"));
}
