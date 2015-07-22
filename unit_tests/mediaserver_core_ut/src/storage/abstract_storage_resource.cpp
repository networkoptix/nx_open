#include "abstract_storage_resource.h"

#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <gtest/gtest.h>

test::StorageTestGlobals tg;

TEST(AbstractStorageResourceTest, Init)
{
    QString fileStorageUrl = qApp->applicationDirPath() + lit("/tmp");
    QnStorageResource *fileStorage = 
        QnStoragePluginFactory::instance()->createStorage(
            fileStorageUrl
        );
    ASSERT_TRUE(fileStorage);
    fileStorage->setUrl(fileStorageUrl);
    
    tg.storages.push_back(fileStorage);

    QnStorageResource *ftpStorage = 
        QnStoragePluginFactory::instance()->createStorage(
            "ftp://anonymous:@127.0.0.1/tmp",
            false
        );
    ASSERT_TRUE(ftpStorage);
    if (ftpStorage)
        tg.storages.push_back(ftpStorage);
}

TEST(AbstractStorageResourceTest, Capabilities)
{
    for (auto storage : tg.storages)
    {
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

TEST(AbstractStorageResourceTest, FileOperations)
{
    for (auto storage : tg.storages)
    {
    }
}

TEST(AbstractStorageResourceTest, IODevice)
{
    for (auto storage : tg.storages)
    {
        // write, seek
        QString fileName = closeDirPath(storage->getUrl()) + lit("test.tmp");
        std::unique_ptr<QIODevice> ioDevice = std::unique_ptr<QIODevice>(
            storage->open(
                fileName.toLatin1().constData(), 
                QIODevice::WriteOnly
            )
        );
        ASSERT_TRUE(ioDevice);

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
        ASSERT_TRUE(ioDevice);
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
    }
}