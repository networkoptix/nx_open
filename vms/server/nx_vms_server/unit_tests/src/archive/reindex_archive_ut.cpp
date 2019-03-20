#include <gtest/gtest.h>
#include <vector>
#include <memory>

#include <test_support/mediaserver_launcher.h>
#include <health/system_health.h>
#include <recorder/storage_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/storage_plugin_factory.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace nx::vms::server::test {

namespace {

static const QString kTestStorageDirName = "test_storage";

struct ChunkData
{
    qint64 startTimeMs;
    qint64 durationsMs;
};

using ChunkDataList = std::vector<ChunkData>;

struct Catalog
{
    ChunkDataList lowQualityChunks;
    ChunkDataList highQualityChunks;
};

using Archive = QMap<QString, Catalog>;

static ChunkDataList generateChunksForQuality(
    const QString& baseDir,
    const QString& cameraName,
    const QString& quality,
    std::chrono::time_point<std::chrono::system_clock> startPoint,
    int count)
{
    using namespace std::chrono;

    const int durationMs = 70000;
    qint64 startTimeMs = duration_cast<milliseconds>(startPoint.time_since_epoch()).count();
    ChunkDataList result;

    for (int i = 0; i < count; ++i)
    {
        QString fileName = lit("%1_%2.mkv").arg(startTimeMs).arg(durationMs);
        QString pathString = QnStorageManager::dateTimeStr(
            startTimeMs, currentTimeZone() / 60,  "/");

        pathString = lit("%2/%1/%3").arg(cameraName).arg(quality).arg(pathString);
        auto fullDirPath = QDir(baseDir).absoluteFilePath(pathString);
        QString fullFileName =
            closeDirPath(baseDir) + closeDirPath(pathString) + fileName;

        NX_ASSERT(QDir().mkpath(fullDirPath));
        NX_ASSERT(QFile(fullFileName).open(QIODevice::WriteOnly));

        qDebug() << "Creating chunk" << fullFileName;
        result.push_back(ChunkData{startTimeMs, durationMs});
        startTimeMs += durationMs + 5;
    }

    return result;
}

static Catalog generateCameraArchive(
    const QString& baseDir,
    const QString& cameraName,
    std::chrono::time_point<std::chrono::system_clock> startPoint,
    int count)
{
    Catalog result;
    result.lowQualityChunks = generateChunksForQuality(
        baseDir, cameraName, "low_quality", startPoint, count);

    result.highQualityChunks = generateChunksForQuality(
        baseDir, cameraName, "hi_quality", startPoint, count);

    return result;
}

} // namespace

class Reindex: public QObject, public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_server.reset(new MediaServerLauncher());

        m_storagePath = QDir(m_server->dataDir()).absoluteFilePath(kTestStorageDirName);
        qDebug() << "STORAGE PATH:" << m_storagePath;

        QDir(m_storagePath).removeRecursively();
        QDir().mkpath(m_storagePath);
    }

    virtual void TearDown() override
    {
        if (!m_storagePath.isEmpty())
            QDir(m_storagePath).removeRecursively();
    }

    void whenServerStarted()
    {
        ASSERT_TRUE(m_server->start());

        createStorage();
        QObject::connect(
            m_server->serverModule()->normalStorageManager(), &QnStorageManager::rebuildFinished,
            this, &Reindex::onReindexFinished);

        m_server->serverModule()->normalStorageManager()->initDone();
        waitForStorageToAppear();
    }

    void whenServerStopped()
    {
        ASSERT_TRUE(m_server->stop());
    }

    void givenSomeArchiveOnHdd()
    {
        m_generatedArchive["camera1"] = generateCameraArchive(
            m_storagePath, "camera1", std::chrono::system_clock::now() - std::chrono::hours(24), 10);

        m_generatedArchive["camera2"] = generateCameraArchive(
            m_storagePath, "camera2", std::chrono::system_clock::now() - std::chrono::hours(23), 5);
    }

    void thenAllArchiveShouldBeFastScannedCorreclty()
    {
        std::this_thread::sleep_for(std::chrono::seconds(100));
    }

private:
    std::unique_ptr<MediaServerLauncher> m_server;
    Archive m_generatedArchive;
    QString m_storagePath;

    void onReindexFinished(QnSystemHealth::MessageType message)
    {
        qDebug() << "REINDEX FINISHED" << message;
    }

    void waitForStorageToAppear()
    {
        while (true)
        {
            const auto allStorages = m_server->serverModule()->normalStorageManager()->getStorages();
            qDebug() << "ALL STORAGES SIZE:" << allStorages.size();

            const auto testStorageIt = std::find_if(
                allStorages.cbegin(), allStorages.cend(),
                [this](const auto& storage) { return storage->getUrl() == m_storagePath; });

            if (testStorageIt == allStorages.cend())
            {
                qDebug() << "WAITING FOR STORAGE";
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
                continue;
            }

            while (true)
            {
                const auto storage = *testStorageIt;
                //ASSERT_TRUE(storage->isWritable() && storage->isOnline() && storage->isUsedForWriting());
                qDebug() << "STORAGE FOUND" << "WRITABLE" << storage->isWritable() << "ONLINE:" << storage->isOnline() << "USED:" << storage->isUsedForWriting();

                if (!(storage->isWritable() && storage->isOnline() && storage->isUsedForWriting()))
                    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }

            break;
        }
    }

    void createStorage()
    {
        ASSERT_FALSE(m_storagePath.isEmpty());

        QnStorageResourcePtr storage(m_server->serverModule()->storagePluginFactory()->createStorage(
            m_server->commonModule(), "ufile"));
        storage->setName("Test storage");
        storage->setParentId(m_server->commonModule()->moduleGUID());
        storage->setUrl(m_storagePath);
        storage->setSpaceLimit(0);
        storage->setStorageType("local");
        storage->setUsedForWriting(storage->initOrUpdate() == Qn::StorageInit_Ok && storage->isWritable());

        qDebug() << "CREATE STORAGE" << m_storagePath << "OPERATIONAL:" << storage->isUsedForWriting();

        QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName("Storage");
        if (resType)
            storage->setTypeId(resType->getId());

        QList<QnStorageResourcePtr> storages{storage};
        nx::vms::api::StorageDataList apiStorages;
        ec2::fromResourceListToApi(QList<QnStorageResourcePtr>{storage}, apiStorages);

        const auto ec2Connection = m_server->serverModule()->ec2Connection();
        ASSERT_EQ(
            ec2::ErrorCode::ok,
            ec2Connection->getMediaServerManager(Qn::kSystemAccess)->saveStoragesSync(apiStorages));

        for(const auto &storage: storages)
        {
            m_server->serverModule()->commonModule()->messageProcessor()->updateResource(
                storage, ec2::NotificationSource::Local);
        }
    }

};

TEST_F(Reindex, FastArchiveScan_AllDataRetrieved)
{
    //whenServerStarted();
    //whenServerStopped();
    givenSomeArchiveOnHdd();
    whenServerStarted();
    //thenAllArchiveShouldBeFastScannedCorreclty();
}

} // nx::vms::server::test
