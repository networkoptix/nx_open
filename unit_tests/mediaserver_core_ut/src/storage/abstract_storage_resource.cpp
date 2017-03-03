#include <qcoreapplication.h>

#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <sstream>

#include <core/resource_management/resource_properties.h>
#include <memory>
#include <algorithm>
#include <utility>
#include <cstring>
#include <string>
#include <random>

#include <utils/common/long_runnable.h>
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

#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

bool recursiveClean(const QString &path);

class AbstractStorageResourceTest: public ::testing::Test
{
public:
    void SetUp()
    {
        QnStoragePluginFactory::instance()->registerStoragePlugin(
            "file",
            QnFileStorageResource::instance,
            true);
        PluginManager::instance()->loadPlugins(MSSettings::roSettings());

        using namespace std::placeholders;
        for (const auto storagePlugin :
                PluginManager::instance()->findNxPlugins<nx_spl::StorageFactory>(
                    nx_spl::IID_StorageFactory))
        {
            QnStoragePluginFactory::instance()->registerStoragePlugin(
                storagePlugin->storageType(),
                std::bind(
                    &QnThirdPartyStorageResource::instance,
                    _1,
                    storagePlugin),
                false);
        }

        fileStorageUrl = qApp->applicationDirPath() + lit("/tmp");
        QDir().mkpath(fileStorageUrl);

        QnStorageResourcePtr fileStorage = QnStorageResourcePtr(
            QnStoragePluginFactory::instance()->createStorage(
                fileStorageUrl));

        ASSERT_TRUE(fileStorage);
        fileStorage->setUrl(fileStorageUrl);
        ASSERT_TRUE(fileStorage->initOrUpdate());
        storages.push_back(fileStorage);


#if defined (TEST_FTP)
        ftpStorageUrl = ftp://127.0.0.1/test;
        QnStorageResourcePtr ftpStorage = QnStorageResourcePtr(
            QnStoragePluginFactory::instance()->createStorage(
                ftpStorageUrl,
                false));

        ASSERT_TRUE(ftpStorage);
        ftpStorage->setUrl(ftpStorageUrl);

        ASSERT_TRUE(ftpStorage->initOrUpdate())
            << "Ftp storage is unavailable. Check if server is online and url is correct."
            << std::endl;
        storages.push_back(ftpStorage);
#endif // TEST_FTP

#if defined (TEST_SMB)
        smbStorageUrl = smb://127.0.0.1/test;
        QnStorageResourcePtr smbStorage = QnStorageResourcePtr(
            QnStoragePluginFactory::instance()->createStorage(
                smbStorageUrl));

        ASSERT_TRUE(smbStorage);
        smbStorage->setUrl(smbStorageUrl);

        ASSERT_TRUE(smbStorage->initOrUpdate())
            << "Smb storage is unavailable."
            << std::endl;
        storages.push_back(smbStorage);
#endif // TEST_SMB
    }

    ~AbstractStorageResourceTest()
    {
        recursiveClean(qApp->applicationDirPath() + lit("/tmp"));
    }

    QnCommonModule commonModule;
    PluginManager pluginManager;
    std::vector<QnStorageResourcePtr> storages;

    QString fileStorageUrl;

#if !defined (Q_OS_WIN)
    QnPlatformAbstraction platformAbstraction;
#endif
};

TEST_F(AbstractStorageResourceTest, Capabilities)
{
    for (auto storage : storages)
    {
        std::cout << "Storage: " << storage->getUrl().toStdString() << std::endl;
        // storage general functions
        int storageCapabilities = storage->getCapabilities();
        ASSERT_TRUE(storageCapabilities | QnAbstractStorageResource::cap::ListFile);
        ASSERT_TRUE(storageCapabilities | QnAbstractStorageResource::cap::RemoveFile);
        ASSERT_TRUE(storageCapabilities | QnAbstractStorageResource::cap::ReadFile);
        ASSERT_TRUE(storageCapabilities | QnAbstractStorageResource::cap::WriteFile);

        ASSERT_TRUE(storage->initOrUpdate());
        ASSERT_TRUE(storage->getFreeSpace() > 0);
        ASSERT_TRUE(storage->getTotalSpace() > 0);
    }
}

TEST_F(AbstractStorageResourceTest, StorageCommonOperations)
{
    const size_t fileCount = 500;
    std::vector<QString> fileNames;
    const char *dummyData = "abcdefgh";
    const int dummyDataLen = strlen(dummyData);

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
        pathStream << randomString() << ".tmp";
        return pathStream.str();
    };

    for (auto storage : storages)
    {
        std::cout << "Storage: " << storage->getUrl().toStdString() << std::endl;

        fileNames.clear();
        // create many files
        for (size_t i = 0; i < fileCount; ++i)
        {   
            QString fileName = closeDirPath(storage->getUrl()) + QString::fromStdString(randomFilePath());
            fileNames.push_back(fileName);
            std::unique_ptr<QIODevice> ioDevice = std::unique_ptr<QIODevice>(
                    storage->open(fileName.toLatin1().constData(),
                                  QIODevice::WriteOnly));

            ASSERT_TRUE((bool)ioDevice)
                << "ioDevice creation failed: " 
                << fileName.toStdString() 
                << std::endl;

            ASSERT_TRUE(ioDevice->write(dummyData, dummyDataLen) == dummyDataLen);
        }

        // check if newly created files exist and their sizes are correct
        for (const auto &fname : fileNames)
        {   
            ASSERT_TRUE(storage->isFileExists(fname));
            ASSERT_TRUE(storage->getFileSize(fname) == dummyDataLen);
        }

        // remove all
        QnAbstractStorageResource::FileInfoList rootFileList = storage->getFileList(storage->getUrl());

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
        ASSERT_TRUE(rootFileList.isEmpty() || (rootFileList.size() == 1 && 
                                               rootFileList.at(0).fileName().indexOf(".sql") != -1));

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

        ASSERT_TRUE(ioDevice->write(dummyData, dummyDataLen) == dummyDataLen);
        ioDevice.reset();
        ASSERT_TRUE(storage->renameFile(fileName, newFileName));
        ASSERT_TRUE(storage->removeFile(newFileName));
    }
}

TEST_F(AbstractStorageResourceTest, IODevice)
{
    for (auto storage : /*storageManager->getStorages()*/storages)
    {
        // write, seek
        QString fileName = closeDirPath(storage->getUrl()) + lit("test.tmp");
        std::unique_ptr<QIODevice> ioDevice = std::unique_ptr<QIODevice>(
            storage->open(
                fileName.toLatin1().constData(), 
                QIODevice::WriteOnly
            )
        );
        ASSERT_TRUE((bool)ioDevice);

        const int dataSize = 10*1024*1024;
        const int seekCount = 10000;
        const char* newData = "bcdefg";
        const int newDataSize = strlen(newData);
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
        ASSERT_EQ(ioDevice->write(data.data(), dataSize), dataSize);

        std::uniform_int_distribution<> seekDistribution(0, dataSize - 32);

        for (size_t i = 0; i < seekCount; ++i)
        {
            const qint64 seekPos = seekDistribution(gen);
            ASSERT_TRUE(ioDevice->seek(seekPos));
            ASSERT_TRUE(ioDevice->write(newData, newDataSize) == newDataSize);
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
    virtual QIODevice* open(const QString&, QIODevice::OpenMode) override {
        return nullptr;
    }

    virtual float getAvarageWritingUsage() const override {
        assert(0);
        return 0.0;
    }

    virtual QnAbstractStorageResource::FileInfoList getFileList(const QString&) override {
        assert(0);
        return QnAbstractStorageResource::FileInfoList();
    }

    virtual qint64 getFileSize(const QString&) const override {
        assert(0);
        return 0;
    }

    virtual bool removeFile(const QString&) override {
        assert(0);
        return true;
    }

    virtual bool removeDir(const QString&) override {
        assert(0);
        return true;
    }

    virtual bool renameFile(const QString&, const QString&) override {
        assert(0);
        return true;
    }

    virtual bool isFileExists(const QString&) override {
        return true;
    }

    virtual bool isDirExists(const QString&) override {
        assert(0);
        return true;
    }

    virtual qint64 getFreeSpace() override {
        assert(0);
        return 0;
    }

    virtual int getCapabilities() const override {
        assert(0);
        return 0;
    }

    virtual bool initOrUpdate() const override {
        assert(0);
        return true;
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
    virtual qint64 getTotalSpace() override {
        return (qint64)10 * 1024 * 1024 * 1024;
    }
};

class MockStorageResource2 : public AbstractMockStorageResource {
public:
    virtual qint64 getTotalSpace() override {
        return (qint64)20 * 1024 * 1024 * 1024;
    }
};

class MockStorageResource3 : public AbstractMockStorageResource {
public:
    virtual qint64 getTotalSpace() override {
        return (qint64)30 * 1024 * 1024 * 1024;
    }
};

TEST_F(AbstractStorageResourceTest, LoadBalancingAlgorithmTest)
{
    QnStorageManager storageManager(QnServer::StoragePool::Normal);
    Q_UNUSED(storageManager);

    QnStorageResourcePtr storage1 = QnStorageResourcePtr(new MockStorageResource1);
    storage1->setUrl(fileStorageUrl);
    storage1->setId("{45FF0AD9-649B-4EDC-B032-13603EA37077}");
    storage1->setUsedForWriting(true);

    QnStorageResourcePtr storage2 = QnStorageResourcePtr(new MockStorageResource2);
    storage2->setUrl(fileStorageUrl);
    storage2->setId("{22E3AD7E-F4E7-4AE5-AD70-0790B05B4566}");
    storage2->setUsedForWriting(true);

    QnStorageResourcePtr storage3 = QnStorageResourcePtr(new MockStorageResource3);
    storage3->setUrl(fileStorageUrl);
    storage3->setId("{30E7F3EA-F4DB-403F-B9DD-66A38DA784CF}");
    storage3->setUsedForWriting(true);

    qnNormalStorageMan->addStorage(storage1);
    qnNormalStorageMan->addStorage(storage2);
    qnNormalStorageMan->addStorage(storage3);

    storage1->setStatus(Qn::Online);
    storage2->setStatus(Qn::Online);
    storage3->setStatus(Qn::Online);

    storage1->setWritedCoeff(0.3);
    storage1->setWritedCoeff(0.3);
    storage1->setWritedCoeff(0.4);

    const int writeCount = 5000;
    QnUuid currentStorageId;

    int currentStorageUseCount = 0;
    int useInARowOverflowCount = 0;

    const int kMaxStorageUseInARow = 15;
    const int kMaxUseInARowOverflowCount = 5;
    const double kMaxUsageDelta = 50;

    for (int i = 0; i < writeCount; ++i)
    {
        auto storage = qnNormalStorageMan->getOptimalStorageRoot(nullptr);
        ASSERT_TRUE(storage);
        if (currentStorageId != storage->getId())
        {
            currentStorageId = storage->getId();
            if (currentStorageUseCount > kMaxStorageUseInARow)
                ++useInARowOverflowCount;
            currentStorageUseCount = 0;
        }
        else
        {
            ++currentStorageUseCount;
        }
        storage->addWrited(1000.0);
    }
    /*  Actually, due to probabilistic nature of selecting storage algorithm
    *   some storage may be selected more than kMaxStorageUseIARow times.
    *   Let's at least check that such peaks are not too often.
    *   kMaxUseInARowOverflowCount peak breaches on 1 * 1000 * 1000 selections seem fair enough.
    */
    ASSERT_TRUE(useInARowOverflowCount < kMaxUseInARowOverflowCount);

    double storage1UsageCoeff = storage1->calcUsageCoeff();
    double storage2UsageCoeff = storage2->calcUsageCoeff();
    double storage3UsageCoeff = storage3->calcUsageCoeff();

    ASSERT_TRUE(qAbs(storage1UsageCoeff - storage2UsageCoeff) < kMaxUsageDelta);
    ASSERT_TRUE(qAbs(storage1UsageCoeff - storage3UsageCoeff) < kMaxUsageDelta);
    ASSERT_TRUE(qAbs(storage3UsageCoeff - storage2UsageCoeff) < kMaxUsageDelta);
}
