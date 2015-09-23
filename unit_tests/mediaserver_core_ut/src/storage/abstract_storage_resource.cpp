#include "abstract_storage_resource.h"
#include <qcoreapplication.h>

#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <sstream>
#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

std::unique_ptr<test::StorageTestGlobals> tg;

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
    QnStorageResource *fileStorage = 
        QnStoragePluginFactory::instance()->createStorage(
            fileStorageUrl
        );
    ASSERT_TRUE(fileStorage);
    fileStorage->setUrl(fileStorageUrl);

    tg->storages.push_back(fileStorage);

    if (!tg->ftpStorageUrl.isEmpty())
    {
        QnStorageResource *ftpStorage =
            QnStoragePluginFactory::instance()->createStorage(
                tg->ftpStorageUrl,
                false
            );
        EXPECT_TRUE(ftpStorage && ftpStorage->isAvailable());
        if (ftpStorage && ftpStorage->isAvailable())
        {
            ftpStorage->setUrl(tg->ftpStorageUrl);
            tg->storages.push_back(ftpStorage);
        }
        else
            std::cout
                << "Ftp storage is unavailable. Check if server is online and url is correct."
                << std::endl;
    }

    if (!tg->smbStorageUrl.isEmpty())
    {
        QnStorageResource *smbStorage =
            QnStoragePluginFactory::instance()->createStorage(
                tg->smbStorageUrl
            );
        EXPECT_TRUE(smbStorage);
        smbStorage->setUrl(tg->smbStorageUrl);

        tg->storages.push_back(smbStorage);
    }
}

TEST(AbstractStorageResourceTest, Capabilities)
{
    for (auto storage : tg->storages)
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

        for (size_t i = 0; i < pathDistribution(gen) - 1; ++i)
        {
            pathStream << randomString() << "/";
        }
        pathStream << randomString() << ".tmp";
        return pathStream.str();
    };

    for (auto storage : tg->storages)
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
        ASSERT_TRUE(rootFileList.isEmpty());

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
    for (auto storage : tg->storages)
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

TEST(AbstractStorageResourceTest, fini)
{
    tg.reset();
}
