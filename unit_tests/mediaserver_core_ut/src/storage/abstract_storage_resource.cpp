#include <memory>
#include <algorithm>
#include <utility>
#include <cstring>
#include <string>
#include <random>
#include <unordered_map>

#include <utils/common/long_runnable.h>
#include <utils/common/writer_pool.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>
#include <recorder/file_deletor.h>
#include <recorder/storage_manager.h>
#include <plugins/plugin_manager.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <plugins/storage/third_party_storage_resource/third_party_storage_resource.h>
#include <media_server/settings.h>
#include <core/resource_management/status_dictionary.h>
#include "utils/common/util.h"
#include <common/common_module.h>

#ifndef _WIN32
#   include <platform/monitoring/global_monitor.h>
#   include <platform/platform_abstraction.h>
#endif
#include <qcoreapplication.h>

#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <sstream>
#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

#include <core/resource_management/resource_properties.h>
#include "../utils.h"

extern nx::ut::cfg::Config config;

namespace
{

class AbstractStorageResourceTest: public ::testing::Test
{
protected:

    virtual void SetUp() override
    {
        prepare();

        ASSERT_TRUE((bool)workDirResource.getDirName());

        QString fileStorageUrl = *workDirResource.getDirName();
        QnStorageResourcePtr fileStorage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(fileStorageUrl));
        fileStorage->setUrl(fileStorageUrl);
        ASSERT_TRUE(fileStorage && fileStorage->initOrUpdate() == Qn::StorageInit_Ok);

        storageManager->addStorage(fileStorage);

        if (!config.ftpUrl.isEmpty())
        {
            QnStorageResourcePtr ftpStorage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(config.ftpUrl, false));
            EXPECT_TRUE(ftpStorage && ftpStorage->initOrUpdate() == Qn::StorageInit_Ok) << "Ftp storage is unavailable. Check if server is online and url is correct." << std::endl;
        }

        if (!config.smbUrl.isEmpty())
        {
            QnStorageResourcePtr smbStorage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(config.smbUrl));
            EXPECT_TRUE(smbStorage && smbStorage->initOrUpdate() == Qn::StorageInit_Ok);
            smbStorage->setUrl(smbStorageUrl);
            storageManager->addStorage(smbStorage);
        }
   }

    void prepare()
    {
        MSSettings::initializeROSettings();

        this->ftpStorageUrl = config.ftpUrl;
        this->smbStorageUrl = config.smbUrl;

        resourcePool = std::unique_ptr<QnResourcePool>(new QnResourcePool);

        commonModule = std::unique_ptr<QnCommonModule>(new QnCommonModule);
        commonModule->setModuleGUID(QnUuid("6F789D28-B675-49D9-AEC0-CEFFC99D674E"));

        storageManager = std::unique_ptr<QnStorageManager>(new QnStorageManager(QnServer::StoragePool::Normal));
        fileDeletor = std::unique_ptr<QnFileDeletor>(new QnFileDeletor);
        pluginManager = std::unique_ptr<PluginManager>( new PluginManager);

        platformAbstraction = std::unique_ptr<QnPlatformAbstraction>(new QnPlatformAbstraction(0));

        QnStoragePluginFactory::instance()->registerStoragePlugin("file", QnFileStorageResource::instance, true);
        PluginManager::instance()->loadPlugins(MSSettings::roSettings());

        for (const auto storagePlugin : PluginManager::instance()->findNxPlugins<nx_spl::StorageFactory>(nx_spl::IID_StorageFactory))
        {
            QnStoragePluginFactory::instance()->registerStoragePlugin(
                storagePlugin->storageType(),
                std::bind(
                    &QnThirdPartyStorageResource::instance,
                    std::placeholders::_1,
                    storagePlugin
                ),
                false
            );
        }
    }

    QString                             ftpStorageUrl;
    QString                             smbStorageUrl;
    QnWriterPool writerPool;
    std::unique_ptr<QnFileDeletor>      fileDeletor;
    std::unique_ptr<QnStorageManager>   storageManager;
    std::unique_ptr<QnResourcePool>     resourcePool;
    std::unique_ptr<QnCommonModule>     commonModule;
    std::unique_ptr<PluginManager>      pluginManager;
    QnResourceStatusDictionary          rdict;

    nx::ut::utils::WorkDirResource workDirResource;

    std::unique_ptr<QnPlatformAbstraction > platformAbstraction;
};
} // namespace <anonymous>


TEST_F(AbstractStorageResourceTest, Capabilities)
{
    for (auto storage : storageManager->getStorages())
    {
        std::cout << "Storage: " << storage->getUrl().toStdString() << std::endl;
        // storage general functions
        int storageCapabilities = storage->getCapabilities();
        ASSERT_TRUE(storageCapabilities | QnAbstractStorageResource::cap::ListFile);
        ASSERT_TRUE(storageCapabilities | QnAbstractStorageResource::cap::RemoveFile);
        ASSERT_TRUE(storageCapabilities | QnAbstractStorageResource::cap::ReadFile);
        ASSERT_TRUE(storageCapabilities | QnAbstractStorageResource::cap::WriteFile);

        ASSERT_TRUE(storage->getFreeSpace() > 0);
        ASSERT_TRUE(storage->getTotalSpace() > 0);
    }
}

TEST_F(AbstractStorageResourceTest, StorageCommonOperations)
{
    const size_t fileCount = 500;
    std::vector<QString> fileNames;
    const char *dummyData = "abcdefgh";
    const size_t dummyDataLen = strlen(dummyData);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> nameDistribution(0, 15);
    std::uniform_int_distribution<> pathDistribution(1, 5);
    std::stringstream pathStream;
    std::stringstream randomStringStream;

    auto randomString = [&]
    {
        randomStringStream.clear();
        randomStringStream.str("");

        for (size_t i = 0; i < 16; ++i)
            randomStringStream << std::hex << nameDistribution(gen);

        return randomStringStream.str();
    };

    auto randomFilePath = [&]
    {
        pathStream.clear();
        pathStream.str("");

        for (int i = 0; i < pathDistribution(gen) - 1; ++i)
        {
            pathStream << randomString() << "/";
        }
        pathStream << randomString() << ".testfile";
        return pathStream.str();
    };

    for (auto storage : storageManager->getStorages())
    {
        std::cout << "Storage: " << storage->getUrl().toStdString() << std::endl;

        fileNames.clear();
        // create many files
        for (size_t i = 0; i < fileCount; ++i)
        {
            QString fileName = closeDirPath(storage->getUrl()) + QString::fromStdString(randomFilePath());
            fileNames.push_back(fileName);
            std::unique_ptr<QIODevice> ioDevice = std::unique_ptr<QIODevice>(
                storage->open(
                    fileName.toLatin1().constData(),
                    QIODevice::WriteOnly
                )
            );
            ASSERT_TRUE((bool)ioDevice)
                << "ioDevice creation failed: "
                << fileName.toStdString()
                << std::endl;

            ASSERT_EQ(ioDevice->write(dummyData, dummyDataLen), (int)dummyDataLen);
        }

        // check if newly created files exist and their sizes are correct
        for (const auto &fname : fileNames)
        {
            ASSERT_TRUE(storage->isFileExists(fname));
            ASSERT_EQ(storage->getFileSize(fname), (int)dummyDataLen);
        }

        // remove all
        QnAbstractStorageResource::FileInfoList rootFileList = storage->getFileList(
            storage->getUrl()
        );
        ASSERT_TRUE(!rootFileList.isEmpty());

        std::function<void (const QnAbstractStorageResource::FileInfoList &)> recursiveRemover =
            [&](const QnAbstractStorageResource::FileInfoList &fl)
            {
                for (const auto& entry : fl)
                {
                    if (entry.isDir())
                    {
                        ASSERT_TRUE(storage->isDirExists(entry.absoluteFilePath()));
                        QnAbstractStorageResource::FileInfoList dirFileList =
                            storage->getFileList(
                                entry.absoluteFilePath()
                            );
                        if (!dirFileList.isEmpty())
                            recursiveRemover(dirFileList);
                        ASSERT_TRUE(storage->removeDir(entry.absoluteFilePath()));
                    }
                    else
                    {
                        ASSERT_TRUE(storage->removeFile(entry.absoluteFilePath()));
                    }
                }
            };
        recursiveRemover(rootFileList);
        rootFileList = storage->getFileList(storage->getUrl());
        bool hasTestFilesLeft = std::any_of(
            rootFileList.cbegin(),
            rootFileList.cend(),
            [](const QnAbstractStorageResource::FileInfo& fi)
            {
                return fi.fileName().indexOf(".testfile") != -1;
            }
        );
        ASSERT_TRUE(rootFileList.isEmpty() || !hasTestFilesLeft);

        // rename file
        QString fileName = closeDirPath(storage->getUrl()) + lit("old_name.tmp");
        QString newFileName = closeDirPath(storage->getUrl()) + lit("new_name.tmp");
        std::unique_ptr<QIODevice> ioDevice = std::unique_ptr<QIODevice>(
            storage->open(
                fileName.toLatin1().constData(),
                QIODevice::WriteOnly
            )
        );

        ASSERT_TRUE((bool)ioDevice)
            << "ioDevice creation failed: "
            << fileName.toStdString()
            << std::endl;

        ASSERT_EQ(ioDevice->write(dummyData, dummyDataLen), (int)dummyDataLen);
        ioDevice.reset();
        ASSERT_TRUE(storage->renameFile(fileName, newFileName));
        ASSERT_TRUE(storage->removeFile(newFileName));
    }
}

TEST_F(AbstractStorageResourceTest, IODevice)
{
    for (auto storage : storageManager->getStorages())
    {
        std::cout << "Storage: " << storage->getUrl().toStdString() << std::endl;

        // write, seek
        QString fileName = closeDirPath(storage->getUrl()) + lit("test.tmp");
        std::unique_ptr<QIODevice> ioDevice = std::unique_ptr<QIODevice>(
            storage->open(
                fileName.toLatin1().constData(),
                QIODevice::WriteOnly
            )
        );
        ASSERT_TRUE((bool)ioDevice);

        const size_t dataSize = 10*1024*1024;
        const size_t seekCount = 10000;
        const char* newData = "bcdefg";
        const size_t newDataSize = strlen(newData);
        std::vector<char> data(dataSize);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dataDistribution(-127, 127);

        std::generate(data.begin(), data.end(),
            [&dataDistribution, &gen]
            {
                return dataDistribution(gen);
            }
        );
        ASSERT_TRUE(ioDevice->write(data.data(), dataSize) == dataSize);

        std::uniform_int_distribution<> seekDistribution(0, dataSize - 32);

        for (size_t i = 0; i < seekCount; ++i)
        {
            const qint64 seekPos = seekDistribution(gen);
            ASSERT_TRUE(ioDevice->seek(seekPos));
            ASSERT_EQ(newDataSize, ioDevice->write(newData, newDataSize));
            std::copy(newData, newData + newDataSize, data.begin() + seekPos);
        }

        ioDevice->close();
        ioDevice.reset();

        // IODevice
        // read, check written before data
        ioDevice = std::unique_ptr<QIODevice>(
            storage->open(
                fileName.toLatin1().constData(),
                QIODevice::ReadOnly
            )
        );
        ASSERT_TRUE((bool)ioDevice);
        QByteArray readData = ioDevice->readAll();
        ASSERT_TRUE(readData.size() == dataSize);
        if (memcmp(readData.constData(), data.data(), dataSize) != 0)
        {
            for (size_t i = 0; i < dataSize; ++i)
            {
                if (readData.at(i) != data[i])
                {
                    std::cout  << i << ":"     << std::hex
                               << "readData: " << (unsigned)readData.at(i) << "    "
                               << "data: "     << (unsigned)data[i]        << std::endl;
                }
            }
        }

        ASSERT_TRUE(memcmp(readData.constData(), data.data(), dataSize) == 0);

        // random seeks/reads
        const size_t readSize = 10;
        for (size_t i = 0; i < seekCount; ++i)
        {
            const qint64 seekPos = seekDistribution(gen);
            ASSERT_TRUE(ioDevice->seek(seekPos));
            QByteArray buf = ioDevice->read(readSize);
            ASSERT_TRUE(memcmp(buf.constData(), data.data(), readSize));
        }

        ioDevice->close();
        ASSERT_TRUE(storage->removeFile(fileName));
    }
}

class AbstractMockStorageResource : public QnStorageResource {
public:
	std::atomic<qint64> freeSpace;

	AbstractMockStorageResource(qint64 freeSpace) : freeSpace(freeSpace) {}

    virtual QIODevice* open(const QString&, QIODevice::OpenMode) override {
        return nullptr;
    }

    virtual float getAvarageWritingUsage() const override {
        NX_ASSERT(0);
        return 0.0;
    }

    virtual QnAbstractStorageResource::FileInfoList getFileList(const QString&) override {
        NX_ASSERT(0);
        return QnAbstractStorageResource::FileInfoList();
    }

    virtual qint64 getFileSize(const QString&) const override {
        NX_ASSERT(0);
        return 0;
    }

    virtual bool removeFile(const QString&) override {
        NX_ASSERT(0);
        return true;
    }

    virtual bool removeDir(const QString&) override {
        NX_ASSERT(0);
        return true;
    }

    virtual bool renameFile(const QString&, const QString&) override {
        NX_ASSERT(0);
        return true;
    }

    virtual bool isFileExists(const QString&) override {
        return true;
    }

    virtual bool isDirExists(const QString&) override {
        NX_ASSERT(0);
        return true;
    }

    virtual qint64 getFreeSpace() override {
        NX_ASSERT(0);
        return 0;
    }

    virtual int getCapabilities() const override {
        return QnAbstractStorageResource::cap::DBReady || QnAbstractStorageResource::cap::ReadFile ||
            QnAbstractStorageResource::cap::WriteFile || QnAbstractStorageResource::cap::RemoveFile ||
            QnAbstractStorageResource::cap::ListFile;
    }

    virtual Qn::StorageInitResult initOrUpdate() override {
        NX_ASSERT(0);
        return Qn::StorageInit_Ok;
    }

    virtual void setUrl(const QString& url) override {
        QnStorageResource::setUrl(url);
    }

private:
    virtual QString getPath() const override {
        return QnStorageResource::getPath();
    }
};

class MockStorageResource1 : public AbstractMockStorageResource {
public:
	MockStorageResource1() : AbstractMockStorageResource((qint64)3 * 1024 * 1024 * 1024 * 1024) {}

    virtual qint64 getTotalSpace() const override {
        return (qint64)3 * 1024 * 1024 * 1024 * 1024;
    }

    virtual qint64 getFreeSpace() override {
        return freeSpace;
    }
};

class MockStorageResource2 : public AbstractMockStorageResource {
public:
	MockStorageResource2() : AbstractMockStorageResource((qint64)3 * 1024 * 1024 * 1024 * 1024) {}

    virtual qint64 getTotalSpace() const override {
        return (qint64)3 * 1024 * 1024 * 1024 * 1024;
    }

    virtual qint64 getFreeSpace() override {
        return freeSpace;
    }
};

class MockStorageResource3 : public AbstractMockStorageResource {
public:
	MockStorageResource3() : AbstractMockStorageResource((qint64)3 * 1024 * 1024 * 1024 * 1024) {}

    virtual qint64 getTotalSpace() const override {
        return (qint64)3 * 1024 * 1024 * 1024 * 1024;
    }

    virtual qint64 getFreeSpace() override {
		return freeSpace;
    }
};

struct OccupiedSpaceAccess
{
	static void addSpaceInfoOccupiedValue(QnStorageManager* storageManager, int storageIndex, qint64 value)
	{
		storageManager->addSpaceInfoOccupiedValue(storageIndex, value);
	}

	static void subtractSpaceInfoOccupiedValue(QnStorageManager* storageManager, int storageIndex, qint64 value)
	{
		storageManager->subtractSpaceInfoOccupiedValue(storageIndex, value);
	}

	static qint64 getSpaceInfoOccupiedValue(QnStorageManager* storageManager, int storageIndex)
	{
		return storageManager->getSpaceInfoOccupiedValue(storageIndex);
	}

	static void setSpaceInfoUsageCoeff(QnStorageManager* storageManager, int storageIndex, double coeff)
	{
		storageManager->setSpaceInfoUsageCoeff(storageIndex, coeff);
	}

	static double getSpaceInfoUsageCoeff(QnStorageManager* storageManager, int storageIndex)
	{
		return storageManager->getSpaceInfoUsageCoeff(storageIndex);
	}
};

struct StorageUseStats
{
    int selected;
    qint64 written;
    int selectInARowOverflow;
    StorageUseStats() : 
        selected(0), 
        written(0),
        selectInARowOverflow(0) {}
};

using StorageUseStatsMap = std::unordered_map<int, StorageUseStats>;

const char* filler()
{
    return "\t\t\t\t";
}

void printStorageUseStatsHeader(int storagesCount,
                                int recordersCount,
                                int writtenBlock,
                                int totalWrites,
                                int overflowLimit)
{
    printf("\n\n%sStorage selection algorithms test\n\n", filler());
    printf("Storages count:                     %d\n", storagesCount);
    printf("Write block size:                   %d\n", writtenBlock);
    printf("Parallel recorders count:           %d\n", recordersCount);
    printf("Total writes:                       %d\n", totalWrites);
    printf("Select same storage in a row limit: %d\n", overflowLimit);
    printf("\n");
}

class StorageUseStatsPrinter {
public:
    StorageUseStatsPrinter(const StorageUseStatsMap& statsMap, 
                           int recordersCount,
                           int writtenBlock,
                           int totalWrites,
                           int overflowLimit) :
        m_statsMap(statsMap)
    {
        printStorageUseStatsHeader(m_statsMap.size(),
                                   recordersCount,
                                   writtenBlock,
                                   totalWrites,
                                   overflowLimit);
    }

    void operator()() const
    {
        printTableHeader();
        printStoragesStats();
    }

private:
    void printTableHeader() const
    {
        for (const auto& statEntry: m_statsMap)
            printf("Storage %d%s", statEntry.first, filler());

        printf("\n");
    }    

    void printStoragesStats() const
    {
        printSelected();
        printWritten();
        printOverflow();
    }

    void printSelected() const
    {
        forEachStatsEntry([this] (const StorageUseStats& stats) {
            printf("Selected: %d%s", stats.selected, "\t\t\t");
        });
        printf("\n");
    }

    void printWritten() const
    {
        forEachStatsEntry([this] (const StorageUseStats& stats) {
            printf("Bytes written: %lld%s", stats.written, "\t\t");
        });
        printf("\n");
    }

    void printOverflow() const
    {
        forEachStatsEntry([this] (const StorageUseStats& stats) {
            printf("Overflows: %d%s", stats.selectInARowOverflow, filler());
        });
        printf("\n");
    }

    template<typename F>
    void forEachStatsEntry(F f) const
    {
        for (const auto& entryPair: m_statsMap)
            f(entryPair.second);
    }

private:
    const StorageUseStatsMap& m_statsMap;
};


TEST(Storage_load_balancing_algorithm_test, Main)
{
    MSSettings::initializeROSettings();
    std::unique_ptr<QnCommonModule> commonModule;
    if (!qnCommon) {
        commonModule = std::unique_ptr<QnCommonModule>(new QnCommonModule);
    }
    commonModule->setModuleGUID(QnUuid("{A680980C-70D1-4545-A5E5-72D89E33648B}"));

    std::unique_ptr<QnStorageManager> storageManager;
    if (!qnNormalStorageMan) {
        storageManager = std::unique_ptr<QnStorageManager>(
                new QnStorageManager(QnServer::StoragePool::Normal));
    }
    storageManager->stopAsyncTasks();

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

    nx::ut::utils::WorkDirResource workDirResource;
    ASSERT_TRUE((bool)workDirResource.getDirName());
    QString basePath = *workDirResource.getDirName();

    auto storage1 = QnStorageResourcePtr(new MockStorageResource1);
    QString storage1Path = basePath + lit("/s1");
    ASSERT_TRUE(QDir().mkdir(storage1Path));
    storage1->setUrl(storage1Path);
    storage1->setId(QnUuid("{45FF0AD9-649B-4EDC-B032-13603EA37077}"));
    storage1->setUsedForWriting(true);

    QnStorageResourcePtr storage2 = QnStorageResourcePtr(new MockStorageResource2);
    QString storage2Path = basePath + lit("/s2");
    ASSERT_TRUE(QDir().mkdir(storage2Path));
    storage2->setUrl(storage2Path);
    storage2->setId(QnUuid("{22E3AD7E-F4E7-4AE5-AD70-0790B05B4566}"));
    storage2->setUsedForWriting(true);

    QnStorageResourcePtr storage3 = QnStorageResourcePtr(new MockStorageResource3);
    QString storage3Path = basePath + lit("/s3");
    ASSERT_TRUE(QDir().mkdir(storage3Path));
    storage3->setUrl(storage3Path);
    storage3->setId(QnUuid("{30E7F3EA-F4DB-403F-B9DD-66A38DA784CF}"));
    storage3->setUsedForWriting(true);

    qnNormalStorageMan->addStorage(storage1);
    qnNormalStorageMan->addStorage(storage2);
    qnNormalStorageMan->addStorage(storage3);

    storage1->setStatus(Qn::Online);
    storage2->setStatus(Qn::Online);
    storage3->setStatus(Qn::Online);

    QnUuid currentStorageId;

    const int kMaxStorageUseInARow = 5;
    const int kMaxUseInARowOverflowCount = 5;
    const int kWriteCount = 2 * 100;
	const int kWrittenBlock = 10 * 1024;
	const size_t kRecordersCount = 10;

    int currentStorageUseCount = 0;
    int useInARowOverflowCount = 0;
    int totalWrites = 0;

	std::unordered_map<int, StorageUseStats> storagesUsageData;
	std::mutex testMutex;
	std::vector<nx::utils::thread> recorders;

	for (size_t i = 0; i < kRecordersCount; ++i)
	{
		recorders.emplace_back(nx::utils::thread(
		[&]
		{
			for (int i = 0; i < kWriteCount; ++i)
			{
				auto storage = qnNormalStorageMan->getOptimalStorageRoot(nullptr);
				storage.dynamicCast<AbstractMockStorageResource>()->freeSpace -= kWrittenBlock;
				int storageIndex = qnStorageDbPool->getStorageIndex(storage);
				OccupiedSpaceAccess::addSpaceInfoOccupiedValue(qnNormalStorageMan, storageIndex, kWrittenBlock);

				std::lock_guard<std::mutex> lock(testMutex);
                ++totalWrites;
				++storagesUsageData[storageIndex].selected;
				storagesUsageData[storageIndex].written += kWrittenBlock;

				if (currentStorageId != storage->getId())
				{
					currentStorageId = storage->getId();
					if (currentStorageUseCount > kMaxStorageUseInARow)
                    {
						++useInARowOverflowCount;
                        ++storagesUsageData[storageIndex].selectInARowOverflow;
                    }
					currentStorageUseCount = 0;
				}
				else
				{
					++currentStorageUseCount;
				}
			}
		}));
	}

	for (auto& t: recorders)
		if (t.joinable())
			t.join();

    StorageUseStatsPrinter(storagesUsageData, 
                           kRecordersCount, 
                           kWrittenBlock, 
                           totalWrites,
                           kMaxStorageUseInARow)();

    /*  Actually, due to probabilistic nature of selecting storage algorithm
    *   some storage may be selected more than kMaxStorageUseIARow times.
    *   Let's at least check that such peaks are not too often.
    *   kMaxUseInARowOverflowCount peak breaches on 1 * 1000 * 100 selections seem fair enough.
    */
    ASSERT_TRUE(useInARowOverflowCount < kMaxUseInARowOverflowCount);
    ASSERT_EQ(totalWrites, 2000);
    for (int i = 0; i < 3; ++i)
        ASSERT_LE(qAbs((double)storagesUsageData[i].selected / totalWrites - 0.3), 0.09);
}
