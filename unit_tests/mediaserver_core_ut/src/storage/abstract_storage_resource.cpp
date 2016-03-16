#include "abstract_storage_resource.h"
#include <qcoreapplication.h>

#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <sstream>
#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

#include <core/resource_management/resource_properties.h>

std::unique_ptr<test::StorageTestGlobals> tg;

bool recursiveClean(const QString &path); 

TEST(AbstractStorageResourceTest, Init)
{
    tg.reset(new test::StorageTestGlobals());

    QCommandLineParser parser;
    parser.setApplicationDescription("Test helper");

    QCommandLineOption ftpUrlOption(
        QStringList() << "F" << "ftp-storage-url",
        QCoreApplication::translate("main", "Ftp storage URL."),
        QCoreApplication::translate("main", "URL")
    );
    parser.addOption(ftpUrlOption);

    QCommandLineOption smbUrlOption(
        QStringList() << "S" << "smb-storage-url",
        QCoreApplication::translate("main", "SMB storage URL."),
        QCoreApplication::translate("main", "URL")
    );
    parser.addOption(smbUrlOption);

    parser.process(*QCoreApplication::instance());

    tg->prepare(
        parser.value(
            ftpUrlOption
        ),
        parser.value(
            smbUrlOption
        )
    );


    QString fileStorageUrl = qApp->applicationDirPath() + lit("/tmp");
    QnStorageResourcePtr fileStorage = QnStorageResourcePtr(
        QnStoragePluginFactory::instance()->createStorage(
            fileStorageUrl
        )
    );
    ASSERT_TRUE(fileStorage);
    fileStorage->setUrl(fileStorageUrl);

    tg->storageManager->addStorage(fileStorage);

    if (!tg->ftpStorageUrl.isEmpty())
    {
        QnStorageResourcePtr ftpStorage = QnStorageResourcePtr(
            QnStoragePluginFactory::instance()->createStorage(
                tg->ftpStorageUrl,
                false
            )
        );
        EXPECT_TRUE(ftpStorage && ftpStorage->isAvailable());
        if (ftpStorage && ftpStorage->isAvailable())
        {
            ftpStorage->setUrl(tg->ftpStorageUrl);
            tg->storageManager->addStorage(ftpStorage);
            //tg->storages.push_back(ftpStorage);
        }
        else
            std::cout
                << "Ftp storage is unavailable. Check if server is online and url is correct."
                << std::endl;
    }

    if (!tg->smbStorageUrl.isEmpty())
    {
        QnStorageResourcePtr smbStorage = QnStorageResourcePtr(
            QnStoragePluginFactory::instance()->createStorage(
                tg->smbStorageUrl
            )
        );
        EXPECT_TRUE(smbStorage);
        smbStorage->setUrl(tg->smbStorageUrl);
        tg->storageManager->addStorage(smbStorage);
    }
}

TEST(AbstractStorageResourceTest, Capabilities)
{
    for (auto storage : tg->storageManager->getStorages())
    {
        std::cout << "Storage: " << storage->getUrl().toStdString() << std::endl;
        // storage general functions
        int storageCapabilities = storage->getCapabilities();
        ASSERT_TRUE(storageCapabilities | QnAbstractStorageResource::cap::ListFile);
        ASSERT_TRUE(storageCapabilities | QnAbstractStorageResource::cap::RemoveFile);
        ASSERT_TRUE(storageCapabilities | QnAbstractStorageResource::cap::ReadFile);
        ASSERT_TRUE(storageCapabilities | QnAbstractStorageResource::cap::WriteFile);

        ASSERT_TRUE(storage->isAvailable());
        ASSERT_TRUE(storage->getFreeSpace() > 0);
        ASSERT_TRUE(storage->getTotalSpace() > 0);
    }
}

TEST(AbstractStorageResourceTest, StorageCommonOperations)
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
        pathStream << randomString() << ".tmp";
        return pathStream.str();
    };

    for (auto storage : tg->storageManager->getStorages())
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

            ASSERT_TRUE(ioDevice->write(dummyData, dummyDataLen) == dummyDataLen);
        }

        // check if newly created files exist and their sizes are correct
        for (const auto &fname : fileNames)
        {   
            ASSERT_TRUE(storage->isFileExists(fname));
            ASSERT_TRUE(storage->getFileSize(fname) == dummyDataLen);
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

TEST(AbstractStorageResourceTest, IODevice)
{
    for (auto storage : tg->storageManager->getStorages())
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
        NX_ASSERT(0);
        return 0;
    }

    virtual bool isAvailable() const override {
        NX_ASSERT(0);
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

TEST(Storage_load_balancing_algorithm_test, Main) {
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

    QnStorageResourcePtr storage1 = QnStorageResourcePtr(new MockStorageResource1); 
    storage1->setUrl("url1");
    storage1->setId(QnUuid("{45FF0AD9-649B-4EDC-B032-13603EA37077}"));
    storage1->setUsedForWriting(true);

    QnStorageResourcePtr storage2 = QnStorageResourcePtr(new MockStorageResource2);
    storage2->setUrl("url2");
    storage2->setId(QnUuid("{22E3AD7E-F4E7-4AE5-AD70-0790B05B4566}"));
    storage2->setUsedForWriting(true);

    QnStorageResourcePtr storage3 = QnStorageResourcePtr(new MockStorageResource3);
    storage3->setUrl("url3");
    storage3->setId(QnUuid("{30E7F3EA-F4DB-403F-B9DD-66A38DA784CF}"));
    storage3->setUsedForWriting(true);

    qnNormalStorageMan->addStorage(storage1);
    qnNormalStorageMan->addStorage(storage2);
    qnNormalStorageMan->addStorage(storage3);

    storage1->setStatus(Qn::Online, true);
    storage2->setStatus(Qn::Online, true);
    storage3->setStatus(Qn::Online, true);

    ASSERT_TRUE(storage1->calcUsageCoeff() == 0.0);
    ASSERT_TRUE(storage2->calcUsageCoeff() == 0.0);
    ASSERT_TRUE(storage3->calcUsageCoeff() == 0.0);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int64_t> dist(1 * 1024 * 1024, 50 * 1024 * 1024);
    
    const int writeCount = 1000000;
    int64_t totalWrited = 0;
    QnUuid currentStorageId;
    int currentStorageUseCount = 0;
    const int MAX_STORAGE_USE_IN_A_ROW = 10;
    const int MAX_USAGE_DELTA = 1024;

    for (int i = 0; i < writeCount; ++i) {
        auto storage = qnNormalStorageMan->getOptimalStorageRoot(nullptr);
        ASSERT_TRUE(storage);
        if (currentStorageId != storage->getId()) {
            currentStorageId = storage->getId();
            currentStorageUseCount = 0;
        } else {
            ++currentStorageUseCount;
        }
        ASSERT_TRUE(currentStorageUseCount < MAX_STORAGE_USE_IN_A_ROW);
        int64_t fileSize = dist(gen);
        totalWrited += fileSize;
        storage->addWrited(fileSize);
    }

    ASSERT_TRUE(totalWrited > 0);

    ASSERT_TRUE(qAbs(storage1->calcUsageCoeff() - storage2->calcUsageCoeff()) < MAX_USAGE_DELTA);
    ASSERT_TRUE(qAbs(storage1->calcUsageCoeff() - storage2->calcUsageCoeff()) < MAX_USAGE_DELTA);
    ASSERT_TRUE(qAbs(storage1->calcUsageCoeff() - storage2->calcUsageCoeff()) < MAX_USAGE_DELTA);

    ASSERT_TRUE(qAbs(storage1->calcUsageCoeff() - storage3->calcUsageCoeff()) < MAX_USAGE_DELTA);
    ASSERT_TRUE(qAbs(storage1->calcUsageCoeff() - storage3->calcUsageCoeff()) < MAX_USAGE_DELTA);
    ASSERT_TRUE(qAbs(storage1->calcUsageCoeff() - storage3->calcUsageCoeff()) < MAX_USAGE_DELTA);

    ASSERT_TRUE(qAbs(storage3->calcUsageCoeff() - storage2->calcUsageCoeff()) < MAX_USAGE_DELTA);
    ASSERT_TRUE(qAbs(storage3->calcUsageCoeff() - storage2->calcUsageCoeff()) < MAX_USAGE_DELTA);
    ASSERT_TRUE(qAbs(storage3->calcUsageCoeff() - storage2->calcUsageCoeff()) < MAX_USAGE_DELTA);
}

TEST(AbstractStorageResourceTest, fini)
{
    tg.reset();
    recursiveClean(qApp->applicationDirPath() + lit("/tmp"));
}
