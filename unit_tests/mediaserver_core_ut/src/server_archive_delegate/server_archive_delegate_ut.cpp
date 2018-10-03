#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

extern "C" {
#include <libavformat/avformat.h>
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
#include <utils/media/ffmpeg_initializer.h>
#include <utils/common/writer_pool.h>
#include <nx/utils/log/log.h>
#include <media_server/settings.h>

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
#include <stdio.h>

#include <test_support/utils.h>
#include "media_server_module_fixture.h"
#include <nx/core/access/access_types.h>
#include <test_support/resource/camera_resource_stub.h>

const QString cameraFolder("camera");
const QString lqFolder("low_quality");
const QString hqFolder("hi_quality");

class TestHelper
{
   static const int DEFAULT_TIME_GAP_MS = 15001;

public:
    TestHelper(
        QnMediaServerModule* serverModule,
        QStringList &&paths,
        int fileCount = 0)
    :
        m_storageUrls(std::move(paths)),
        m_timeLine(DEFAULT_TIME_GAP_MS),
        m_fileCount(fileCount)
    {
        generateTestData();
        createStorages(serverModule);
        loadMedia(serverModule);
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

            #if 0 // Debug output
                qDebug() << "current time period: ("
                         << m_currentIt->startTimeMs << " " << m_currentIt->durationMs
                         << lit("%1 left to the end ) ")
                                .arg(m_currentIt->startTimeMs +
                                     m_currentIt->durationMs -
                                     m_timePoint)
                         << "time: " << time << " m_time: " << m_timePoint
                         << " diff: " << std::abs(time - m_timePoint);
            #endif

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
        NX_DEBUG(this, lm("We have %1 files, %2 time periods")
            .arg(m_fileCount).arg(m_timeLine.m_timeLine.size()));

        qint64 prevStartTime;
        int prevDuration;
        qDebug() << "Time periods details: ";

        for (auto it = m_timeLine.m_timeLine.cbegin();
             it != m_timeLine.m_timeLine.cend();
             ++it)
        {
            qDebug() << it->startTimeMs << " " << it->durationMs;
            if (it != m_timeLine.m_timeLine.cbegin())
            {
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
            QDir().mkpath(QDir(m_storageUrls[i]).absoluteFilePath(lit("%2/%1").arg(cameraFolder).arg(lqFolder)));
            QDir().mkpath(QDir(m_storageUrls[i]).absoluteFilePath(lit("%2/%1").arg(cameraFolder).arg(hqFolder)));
        }

        QFile testFile_1("://1445529661948_77158.mkv");
        QFile testFile_2("://1445529880231_64712.mkv");

        int64_t startTimeMs = 1445529661948;
        const int duration_1 = 77158;
        const int duration_2 = 64712;

        auto writeHeaderTimestamp = [](const QString &fileName, int64_t timestamp)
        {
            QFile file(fileName);
            if (!file.setPermissions(QFile::ReadOther | QFile::WriteOther | QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup))
                qDebug() << lit("Set permissions failed for %1").arg(fileName);
            file.open(QIODevice::ReadWrite);
            if (!file.isOpen()) {
                qDebug() << lit("Write header %1 failed").arg(fileName);
                return;
            }
            file.seek(0x219);
            auto buf = file.read(11);
            if (memcmp(buf.constData(), "START_TIMED", 11) != 0) {
                qDebug() << lit("Write header wrong offset %1").arg(fileName);
                return;
            }

            file.seek(0x219 + 0xd);
            file.write(QString::number(timestamp).toLatin1().constData());
            file.close();
        };

        auto copyFile = [&](const QString& root, const QString &quality)
        {
            int curDuration = durationDist(gen) == 0 ? duration_1 : duration_2;
            bool isHole = normalOrHoleDist(gen) <= 1 ? true : false;
            int64_t curStartTime = isHole ? holeStartDistMs(gen) + startTimeMs :
                                            normalStartDistMs(gen) + startTimeMs;
            QString fileName = lit("%1_%2.mkv").arg(curStartTime).arg(curDuration);
            QString pathString = QnStorageManager::dateTimeStr(curStartTime,
                                                               currentTimeZone()/60,
                                                               lit("/"));
            pathString = lit("%2/%1/%3").arg(cameraFolder).arg(quality).arg(pathString);
            auto fullDirPath = QDir(root).absoluteFilePath(pathString);
            decltype(curStartTime + curDuration) error = -1;

            if (!QDir().mkpath(fullDirPath))
            {
                qDebug() << "Create directory" << fullDirPath << "failed";
                return error;
            }
            QString fullFileName = closeDirPath(root) + closeDirPath(pathString)
                                                      + fileName;
            if (curDuration == duration_1)
            {
                if (!testFile_1.copy(fullFileName))
                {
                    qDebug() << "Copy file" << fullFileName << "failed";
                    if (QFile(fullFileName).exists())
                        qDebug() << "Already exsits";
                    return error;
                }
            }
            else
            {
                if (!testFile_2.copy(fullFileName))
                {

                    qDebug() << "Copy file" << fullFileName << "failed";
                    if (QFile(fullFileName).exists())
                        qDebug() << "Already exsits";
                    return error;
                }
            }

            writeHeaderTimestamp(fullFileName, curStartTime);
            m_timeLine.addTimePeriod(curStartTime, curDuration);
            return curStartTime + curDuration;
        };

        for (int i = 0; i < m_fileCount; ++i) {
            int64_t newTime = 0;
            for (int j = 0; j < m_storageUrls.size(); ++j) {
                int64_t d1 = copyFile(m_storageUrls[j], lqFolder);
                int64_t d2 = copyFile(m_storageUrls[j], hqFolder);
                if (d1 == -1 || d2 == -1)
                    continue;
                newTime = newTime > (d1 > d2 ? d1 : d2) ? newTime : (d1 > d2 ? d1 : d2);
            }
            startTimeMs = newTime;
        }
    }

    void createStorages(QnMediaServerModule* serverModule)
    {
        QnStorageManager* normalStorageManager = serverModule->normalStorageManager();
        QnStorageManager* backupStorageManager = serverModule->backupStorageManager();
        for (int i = 0; i < m_storageUrls.size(); ++i)
        {
            QnStorageResourcePtr storage = QnStorageResourcePtr(new QnFileStorageResource(serverModule));
            storage->setUrl(m_storageUrls[i]);
            storage->setId(QnUuid::createUuid());
            storage->setUsedForWriting(true);
            ASSERT_EQ(storage->initOrUpdate(), Qn::StorageInit_Ok);
            if (i % 2 == 0)
                normalStorageManager->addStorage(storage);
            else
                backupStorageManager->addStorage(storage);
            storage->setStatus(Qn::Online);
            m_storages.push_back(storage);
        }
    }

    void loadMedia(QnMediaServerModule* serverModule)
    {
        nx::caminfo::ArchiveCameraDataList archiveCameras;
        for (int i = 0; i < m_storageUrls.size(); ++i)
        {
            QnStorageManager *manager = i % 2 == 0
                ? serverModule->normalStorageManager() : serverModule->backupStorageManager();
            manager->getFileCatalog(lit("%1").arg(cameraFolder), QnServer::LowQualityCatalog);
            manager->getFileCatalog(lit("%1").arg(cameraFolder), QnServer::HiQualityCatalog);
            manager->m_rebuildCancelled = false;

            manager->loadFullFileCatalogFromMedia(m_storages[i], QnServer::LowQualityCatalog, archiveCameras);
            manager->loadFullFileCatalogFromMedia(m_storages[i], QnServer::HiQualityCatalog, archiveCameras);
        }
    }

 private:
    QStringList m_storageUrls;
    TimeLine m_timeLine;
    int m_fileCount;
    std::vector<QnStorageResourcePtr> m_storages;
};

class ServerArchiveDelegatePlaybackTest: public MediaServerModuleFixture
{
};

TEST_F(ServerArchiveDelegatePlaybackTest, TestHelper)
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
    ASSERT_TRUE(timeLine.checkTime(25));
    ASSERT_TRUE(timeLine.checkTime(26));
    ASSERT_TRUE(timeLine.checkTime(28));
}

class ChunkQualityChooserTest: public ::testing::Test
{
public:
    void chooseChunk(bool hqFromBackup, bool lqFromBackup)
    {
        QnVirtualCameraResourcePtr testCamera(new nx::CameraResourceStub());
        testCamera->setId(QnUuid::createUuid());
        testCamera->setPhysicalId(testCamera->getId().toString());

        std::unique_ptr<QnMediaServerModule> serverModule(new QnMediaServerModule());
        serverModule->commonModule()->setModuleGUID(QnUuid("{A680980C-70D1-4545-A5E5-72D89E33648B}"));
        serverModule->normalStorageManager()->stopAsyncTasks();
        serverModule->backupStorageManager()->stopAsyncTasks();

        serverModule->roSettings()->remove(lit("NORMAL_SCAN_ARCHIVE_FROM"));
        serverModule->roSettings()->remove(lit("BACKUP_SCAN_ARCHIVE_FROM"));

        TestHelper testHelper(serverModule.get(), QStringList());

        auto hqStorageManager = hqFromBackup
            ? serverModule->backupStorageManager() : serverModule->normalStorageManager();
        auto lqStorageManager = lqFromBackup
            ? serverModule->backupStorageManager() : serverModule->normalStorageManager();

        DeviceFileCatalogPtr hqCatalog = hqStorageManager->getFileCatalog(
            testCamera->getPhysicalId(), QnServer::ChunksCatalog::HiQualityCatalog);
        DeviceFileCatalogPtr lqCatalog = lqStorageManager->getFileCatalog(
            testCamera->getPhysicalId(), QnServer::ChunksCatalog::LowQualityCatalog);

        auto addRecord = [](DeviceFileCatalogPtr catalog, qint64 startTimeSec, qint64 durationSec)
        {
            DeviceFileCatalog::Chunk chunk;
            chunk.startTimeMs = startTimeSec * 1000;
            chunk.durationMs = durationSec * 1000;
            catalog->addRecord(chunk);
        };

        addRecord(hqCatalog, 0, 100);
        addRecord(hqCatalog, 300, 100);
        addRecord(hqCatalog, 500, 100);

        addRecord(lqCatalog, 0, 102);
        addRecord(lqCatalog, 299, 100 + 1 + 5);
        addRecord(lqCatalog, 506, 100);

        DeviceFileCatalog::UniqueChunkCont ignoreList;
        QnDualQualityHelper qualityHelper(
            serverModule->normalStorageManager(),
            serverModule->backupStorageManager());
        qualityHelper.setResource(testCamera);

        DeviceFileCatalog::TruncableChunk resultChunk;
        DeviceFileCatalogPtr resultCatalog;

        // Before hole LQ > HQ, after hole LQ < HQ, but both <eps/2
        qualityHelper.findDataForTime(
            100 * 1000,
            resultChunk,
            resultCatalog,
            DeviceFileCatalog::OnRecordHole_NextChunk,
            /* precise find*/ false,
            ignoreList);
        ASSERT_TRUE(resultCatalog != nullptr);
        ASSERT_EQ(QnServer::ChunksCatalog::HiQualityCatalog, resultCatalog->getRole());

        qualityHelper.findDataForTime(
            100 * 1000,
            resultChunk,
            resultCatalog,
            DeviceFileCatalog::OnRecordHole_NextChunk,
            /* precise find*/ true,
            ignoreList);
        ASSERT_TRUE(resultCatalog != nullptr);
        ASSERT_EQ(QnServer::ChunksCatalog::LowQualityCatalog, resultCatalog->getRole());

        // Before hole LQ > HQ (duration > eps), after hole LQ > HQ
        qualityHelper.findDataForTime(
            400 * 1000,
            resultChunk,
            resultCatalog,
            DeviceFileCatalog::OnRecordHole_NextChunk,
            /* precise find*/ false,
            ignoreList);
        ASSERT_TRUE(resultCatalog != nullptr);
        ASSERT_EQ(QnServer::ChunksCatalog::LowQualityCatalog, resultCatalog->getRole());

        qualityHelper.findDataForTime(
            400 * 1000,
            resultChunk,
            resultCatalog,
            DeviceFileCatalog::OnRecordHole_NextChunk,
            /* precise find*/ true,
            ignoreList);
        ASSERT_TRUE(resultCatalog != nullptr);
        ASSERT_EQ(QnServer::ChunksCatalog::LowQualityCatalog, resultCatalog->getRole());
    }
};

TEST_F(ChunkQualityChooserTest, main)
{
    chooseChunk(false, false);
    chooseChunk(false, true);
    chooseChunk(true, false);
    chooseChunk(true, true);
}
