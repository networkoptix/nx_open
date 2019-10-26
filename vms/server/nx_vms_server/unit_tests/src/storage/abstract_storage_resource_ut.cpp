#include <memory>
#include <algorithm>
#include <utility>
#include <cstring>
#include <string>
#include <random>
#include <unordered_map>

#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/random_qt_device.h>
#include <utils/common/writer_pool.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>
#include <recorder/storage_manager.h>
#include <plugins/plugin_manager.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <plugins/storage/third_party_storage_resource/third_party_storage_resource.h>
#include <media_server/settings.h>
#include <utils/common/util.h>
#include <media_server/media_server_module.h>

#if !defined(_WIN32)
    #include <platform/platform_abstraction.h>
#endif

#define GTEST_HAS_TR1_TUPLE 0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <sstream>
#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

#include <test_support/utils.h>

#include "media_server_module_fixture.h"

namespace {

class AbstractStorageResourceTest: public MediaServerModuleFixture
{
    using base_type = MediaServerModuleFixture;

protected:
    virtual void TearDown() override
    {
    }

    virtual void SetUp() override
    {
        base_type::SetUp();

        prepare();

        ASSERT_TRUE((bool)workDirResource.getDirName());

        const QString fileStorageUrl = *workDirResource.getDirName();
        QnStorageResourcePtr fileStorage = QnStorageResourcePtr(
            serverModule().storagePluginFactory()->createStorage(
                serverModule().commonModule(),
                fileStorageUrl));
        fileStorage->setUrl(fileStorageUrl);
        ASSERT_TRUE(fileStorage && fileStorage->initOrUpdate() == Qn::StorageInit_Ok);
        serverModule().normalStorageManager()->addStorage(fileStorage);

        if (!nx::ut::cfg::configInstance().ftpUrl.isEmpty())
        {
            const QnStorageResourcePtr ftpStorage = QnStorageResourcePtr(
                serverModule().storagePluginFactory()->createStorage(
                    serverModule().commonModule(),
                    nx::ut::cfg::configInstance().ftpUrl,
                    /*useDefaultForUnknownPrefix*/ false));
            EXPECT_TRUE(ftpStorage && ftpStorage->initOrUpdate() == Qn::StorageInit_Ok)
                << "Ftp storage is unavailable. Check if server is online and url is correct."
                << std::endl;
        }

        if (!nx::ut::cfg::configInstance().smbUrl.isEmpty())
        {
            QnStorageResourcePtr smbStorage = QnStorageResourcePtr(
                serverModule().storagePluginFactory()->createStorage(
                    serverModule().commonModule(),
                    nx::ut::cfg::configInstance().smbUrl));
            EXPECT_TRUE(smbStorage && smbStorage->initOrUpdate() == Qn::StorageInit_Ok);
            smbStorage->setUrl(smbStorageUrl);
            serverModule().normalStorageManager()->addStorage(smbStorage);
        }
   }

    void prepare()
    {
        this->ftpStorageUrl = nx::ut::cfg::configInstance().ftpUrl;
        this->smbStorageUrl = nx::ut::cfg::configInstance().smbUrl;

        pluginManager = std::make_unique<PluginManager>(&serverModule());
        serverModule().storagePluginFactory()->registerStoragePlugin(
            "file",
            [this](QnCommonModule*, const QString& path)
            {
                return QnFileStorageResource::instance(&serverModule(), path);
            }, /*isDefaultProtocol*/ true);
        pluginManager->loadPlugins(serverModule().roSettings());

        for (const auto storagePlugin:
            pluginManager->findNxPlugins<nx_spl::StorageFactory>(nx_spl::IID_StorageFactory))
        {
            const auto settings = &serverModule().settings();
            serverModule().storagePluginFactory()->registerStoragePlugin(
                storagePlugin->storageType(),
                [this, storagePlugin](QnCommonModule* commonModule, const QString& path)
                {
                    auto settings = &serverModule().settings();
                    return QnThirdPartyStorageResource::instance(&serverModule(), path, storagePlugin, settings);
                },
                /*isDefaultProtocol*/ false);
        }
    }

    QString ftpStorageUrl;
    QString smbStorageUrl;
    std::unique_ptr<PluginManager> pluginManager;
    nx::ut::utils::WorkDirResource workDirResource;
};

} // namespace

TEST_F(AbstractStorageResourceTest, Capabilities)
{
    for (auto storage: serverModule().normalStorageManager()->getStorages())
    {
        std::cout << "Storage: " << storage->getUrl().toStdString() << std::endl;
        // storage general functions
        const int storageCapabilities = storage->getCapabilities();
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
    const size_t fileCount = 100;
    std::vector<QString> filenames;
    const char dummyData[] = "abcdefgh";
    const int dummyDataLen = (int) strlen(dummyData);

    nx::utils::random::QtDevice rd;
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

    auto randomFilePath =
        [&]()
        {
            pathStream.clear();
            pathStream.str("");

            for (int i = 0; i < pathDistribution(gen) - 1; ++i)
                pathStream << randomString() << "/";
            pathStream << randomString() << ".testfile";
            return pathStream.str();
        };

    for (auto storage: serverModule().normalStorageManager()->getStorages())
    {
        std::cout << "Storage: " << storage->getUrl().toStdString() << std::endl;

        filenames.clear();

        // Create many files.
        for (size_t i = 0; i < fileCount; ++i)
        {
            const QString fileName =
                closeDirPath(storage->getUrl()) + QString::fromStdString(randomFilePath());
            filenames.push_back(fileName);
            auto ioDevice = std::unique_ptr<QIODevice>(
                storage->open(
                    fileName.toLatin1().constData(),
                    QIODevice::WriteOnly));
            ASSERT_TRUE(ioDevice)
                << "ioDevice creation failed: "
                << fileName.toStdString()
                << std::endl;

            ASSERT_EQ(ioDevice->write(dummyData, dummyDataLen), dummyDataLen);
        }

        // Check if newly created files exist and their sizes are correct.
        for (const auto& filename: filenames)
        {
            ASSERT_TRUE(storage->isFileExists(filename));
            ASSERT_EQ(storage->getFileSize(filename), dummyDataLen);
        }

        // Remove all.
        QnAbstractStorageResource::FileInfoList rootFileList =
            storage->getFileList(storage->getUrl());
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
        const bool hasTestFilesLeft = std::any_of(
            rootFileList.cbegin(),
            rootFileList.cend(),
            [](const QnAbstractStorageResource::FileInfo& fi)
            {
                return fi.fileName().indexOf(".testfile") != -1;
            }
        );
        ASSERT_TRUE(rootFileList.isEmpty() || !hasTestFilesLeft);

        // Rename file.
        const QString fileName = closeDirPath(storage->getUrl()) + "old_name.tmp";
        const QString newFileName = closeDirPath(storage->getUrl()) + "new_name.tmp";
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

        ASSERT_EQ(ioDevice->write(dummyData, dummyDataLen), dummyDataLen);
        ioDevice.reset();
        ASSERT_TRUE(storage->renameFile(fileName, newFileName));
        ASSERT_TRUE(storage->removeFile(newFileName));
    }
}

TEST_F(AbstractStorageResourceTest, IODevice)
{
    for (auto storage: serverModule().normalStorageManager()->getStorages())
    {
        std::cout << "Storage: " << storage->getUrl().toStdString() << std::endl;

        // Write, seek.
        const QString fileName = closeDirPath(storage->getUrl()) + "test.tmp";
        std::unique_ptr<QIODevice> ioDevice = std::unique_ptr<QIODevice>(
            storage->open(
                fileName.toLatin1().constData(),
                QIODevice::WriteOnly
            )
        );
        ASSERT_TRUE((bool)ioDevice);

        const int dataSize = 100 * 1024;
        const size_t seekCount = 1000;
        const char* newData = "bcdefg";
        const size_t newDataSize = strlen(newData);
        std::vector<char> data(dataSize);

        nx::utils::random::QtDevice rd;
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
            for (int i = 0; i < dataSize; ++i)
            {
                if (readData.at(i) != data[i])
                {
                    std::cout << i << ":" << std::hex
                        << "readData: " << (unsigned int) readData.at(i) << "    "
                        << "data: " << (unsigned int) data[i] << std::endl;
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
            ASSERT_EQ(0, memcmp(buf.constData(), data.data() + seekPos, readSize));
        }

        ioDevice->close();
        ASSERT_TRUE(storage->removeFile(fileName));
    }
}

struct ExpectedFileInfo
{
    bool isDir;
    QString baseName;
    QString ext;
    QString dirPath;
    QString fileName;
    QString absolutePath;
};

class FileInfoTest: public ::testing::TestWithParam<ExpectedFileInfo> { };

TEST_P(FileInfoTest, Constructor)
{
    const auto expected = GetParam();
    QnAbstractStorageResource::FileInfo fileInfo(expected.absolutePath, 42, expected.isDir);
    ASSERT_EQ(expected.isDir, fileInfo.isDir());
    ASSERT_EQ(QDir::toNativeSeparators(expected.baseName), fileInfo.baseName());
    ASSERT_EQ(QDir::toNativeSeparators(expected.ext), fileInfo.extension());
    ASSERT_EQ(QDir::toNativeSeparators(expected.dirPath), fileInfo.absoluteDirPath());
    ASSERT_EQ(QDir::toNativeSeparators(expected.fileName), fileInfo.fileName());
    ASSERT_EQ(QDir::toNativeSeparators(expected.absolutePath), fileInfo.absoluteFilePath());
}

INSTANTIATE_TEST_CASE_P(
    FileInfoConstructorTest,
    FileInfoTest,
    ::testing::Values<ExpectedFileInfo>(
        ExpectedFileInfo{false, "file", "ext", "/some/path", "file.ext", "/some/path/file.ext"},
        ExpectedFileInfo{false, "file", "ext", "smb://user:password@host:port/some/path", "file.ext",
            "smb://user:password@host:port/some/path/file.ext"},
        ExpectedFileInfo{true, "dir", "", "/some/path/dir", "dir", "/some/path/dir"},
        ExpectedFileInfo{true, "dir", "", "smb://user:password@host:port/some/path/dir", "dir",
            "smb://user:password@host:port/some/path/dir"}));