#include <memory>
#include <algorithm>
#include <utility>
#include <cstring>
#include <string>
#include <random>
#include <unordered_map>

#include <nx/utils/thread/long_runnable.h>
#include <utils/common/writer_pool.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>
#include <recorder/file_deletor.h>
#include <recorder/storage_manager.h>
#include <recorder/space_info.h>
#include <plugins/plugin_manager.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <plugins/storage/third_party_storage_resource/third_party_storage_resource.h>
#include <media_server/settings.h>
#include <core/resource_management/status_dictionary.h>
#include "utils/common/util.h"
#include <media_server/media_server_module.h>

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
#include <test_support/utils.h>
#include <media_server/media_server_module.h>

namespace
{

class AbstractStorageResourceTest: public ::testing::Test, public QnMediaServerModule
{
public:
    AbstractStorageResourceTest():
        QnMediaServerModule()
    {
    }
protected:

    virtual void TearDown() override
    {
        qnNormalStorageMan->stopAsyncTasks();
        qnBackupStorageMan->stopAsyncTasks();
    }

    virtual void SetUp() override
    {
        prepare();

        ASSERT_TRUE((bool)workDirResource.getDirName());

        QString fileStorageUrl = *workDirResource.getDirName();
        QnStorageResourcePtr fileStorage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(commonModule(), fileStorageUrl));
        fileStorage->setUrl(fileStorageUrl);
        ASSERT_TRUE(fileStorage && fileStorage->initOrUpdate() == Qn::StorageInit_Ok);
        qnNormalStorageMan->addStorage(fileStorage);

        if (!nx::ut::cfg::configInstance().ftpUrl.isEmpty())
        {
            QnStorageResourcePtr ftpStorage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(commonModule(), nx::ut::cfg::configInstance().ftpUrl, false));
            EXPECT_TRUE(ftpStorage && ftpStorage->initOrUpdate() == Qn::StorageInit_Ok) << "Ftp storage is unavailable. Check if server is online and url is correct." << std::endl;
        }

        if (!nx::ut::cfg::configInstance().smbUrl.isEmpty())
        {
            QnStorageResourcePtr smbStorage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(commonModule(), nx::ut::cfg::configInstance().smbUrl));
            EXPECT_TRUE(smbStorage && smbStorage->initOrUpdate() == Qn::StorageInit_Ok);
            smbStorage->setUrl(smbStorageUrl);
            qnNormalStorageMan->addStorage(smbStorage);
        }
   }

    void prepare()
    {
        this->ftpStorageUrl = nx::ut::cfg::configInstance().ftpUrl;
        this->smbStorageUrl = nx::ut::cfg::configInstance().smbUrl;

        commonModule()->setModuleGUID(QnUuid("6F789D28-B675-49D9-AEC0-CEFFC99D674E"));

        pluginManager = std::unique_ptr<PluginManager>( new PluginManager);

        platformAbstraction = std::unique_ptr<QnPlatformAbstraction>(new QnPlatformAbstraction());

        QnStoragePluginFactory::instance()->registerStoragePlugin("file", QnFileStorageResource::instance, true);
        PluginManager::instance()->loadPlugins(roSettings());

        for (const auto storagePlugin : PluginManager::instance()->findNxPlugins<nx_spl::StorageFactory>(nx_spl::IID_StorageFactory))
        {
            QnStoragePluginFactory::instance()->registerStoragePlugin(
                storagePlugin->storageType(),
                std::bind(
                    &QnThirdPartyStorageResource::instance,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    storagePlugin
                ),
                false
            );
        }
    }

    QString                             ftpStorageUrl;
    QString                             smbStorageUrl;
    std::unique_ptr<PluginManager>      pluginManager;
    nx::ut::utils::WorkDirResource workDirResource;

    std::unique_ptr<QnPlatformAbstraction > platformAbstraction;
};
} // namespace <anonymous>

TEST_F(AbstractStorageResourceTest, Capabilities)
{
    for (auto storage : qnNormalStorageMan->getStorages())
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

    for (auto storage : qnNormalStorageMan->getStorages())
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
    for (auto storage : qnNormalStorageMan->getStorages())
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

using StorageDistributionMap = std::unordered_map<int, double>;
using StorageSelectionsMap = std::unordered_map<int, int>;

using namespace nx::recorder;

StorageDistributionMap getStorageDistribution(
    const SpaceInfo& spaceInfo,
    int iterations,
    const std::vector<int>& allowedIndexes)
{
    StorageSelectionsMap selectionsData;
    for (int i = 0; i < iterations; ++i)
        ++selectionsData.emplace(spaceInfo.getOptimalStorageIndex(allowedIndexes), 0).first->second;

    StorageDistributionMap result;
    for (const auto& p: selectionsData)
        result.emplace(p.first, p.second / (double)iterations);

    return result;
}

class StorageBalancingAlgorithmTest : public ::testing::Test
{
protected:
    SpaceInfo spaceInfo;

    virtual void SetUp() override
    {
        spaceInfo.storageAdded(0, 100);
        spaceInfo.storageAdded(1, 100);
        spaceInfo.storageAdded(2, 100);
    }
};

TEST_F(StorageBalancingAlgorithmTest, EqualStorages_NxSpaceNotKnown)
{
    spaceInfo.storageRebuilded(0, 50, 20, 10);
    spaceInfo.storageRebuilded(1, 30, 20, 10);

    /* no storage rebulded call for the third storage. getOptimalStorageIndex() should be
    *  equally distributed
    */
    auto storageDistribution = getStorageDistribution(spaceInfo, 100 * 1000, {0, 1, 2});
    ASSERT_LT(storageDistribution[0] - 0.33, 0.05);
    ASSERT_LT(storageDistribution[1] - 0.33, 0.05);
    ASSERT_LT(storageDistribution[2] - 0.33, 0.05);
}

TEST_F(StorageBalancingAlgorithmTest, EqualStorages_NxSpaceKnown)
{
    spaceInfo.storageRebuilded(0, 50, 20, 10); // Es = 70
    spaceInfo.storageRebuilded(1, 30, 20, 10); // Es = 50
    spaceInfo.storageRebuilded(2, 10, 30, 10); // Es = 40

    /* Total Es = 160 => 0 - 0.4375, 1 - 0.3125, 2 - 0.25 */

    auto storageDistribution = getStorageDistribution(spaceInfo, 100 * 1000, {0, 1, 2});
    ASSERT_LT(storageDistribution[0] - 0.4375, 0.05);
    ASSERT_LT(storageDistribution[1] - 0.3125, 0.05);
    ASSERT_LT(storageDistribution[2] - 0.25, 0.05);
}

TEST_F(StorageBalancingAlgorithmTest, EqualStorages_NxSpaceKnown_OneRemoved)
{
    spaceInfo.storageRebuilded(0, 50, 20, 10); // Es = 70
    spaceInfo.storageRebuilded(1, 30, 20, 10); // Es = 50
    spaceInfo.storageRebuilded(2, 10, 30, 10); // Es = 40, This will be removed

    /* Total Es = 120 => 0 - 0.5833, 1 - 0.4166 */

    spaceInfo.storageRemoved(2);

    auto storageDistribution = getStorageDistribution(spaceInfo, 100 * 1000, {0, 1, 2});
    ASSERT_EQ(storageDistribution.find(2), storageDistribution.cend());
    ASSERT_LT(storageDistribution[0] - 0.5833, 0.05);
    ASSERT_LT(storageDistribution[1] - 0.4166, 0.05);
}


TEST_F(StorageBalancingAlgorithmTest, EqualStorages_NxSpaceKnown_NotAllAllowed)
{
    spaceInfo.storageRebuilded(0, 50, 20, 10); // Es = 70
    spaceInfo.storageRebuilded(1, 30, 20, 10); // Es = 50
    spaceInfo.storageRebuilded(2, 10, 30, 10); // Es = 40. This won't be allowed
    /* Total Es = 120 => 0 - 0.5833, 1 - 0.4166 */

    auto storageDistribution = getStorageDistribution(spaceInfo, 100 * 1000, {0, 1});
    ASSERT_EQ(storageDistribution.find(2), storageDistribution.cend());
    ASSERT_LT(storageDistribution[0] - 0.5833, 0.05);
    ASSERT_LT(storageDistribution[1] - 0.4166, 0.05);
}