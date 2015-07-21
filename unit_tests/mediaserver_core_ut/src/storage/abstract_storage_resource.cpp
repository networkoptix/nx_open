#include <algorithm>
#include <utility>
#include <cstring>
#include <string>

#define GTEST_HAS_TR1_TUPLE 0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <gtest/gtest.h>

#include <core/resource/storage_plugin_factory.h>
#include <recorder/storage_manager.h>
#include <plugins/plugin_manager.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <plugins/storage/third_party_storage_resource/third_party_storage_resource.h>
#include <media_server/settings.h>
#include <core/resource_management/status_dictionary.h>
#include "utils/common/util.h"


TEST(AbstractStorageResourceTest, all)
{
    //init storage manager global instance
    std::unique_ptr<QnStorageManager> storageManager = std::unique_ptr<QnStorageManager>(
        new QnStorageManager
    );

    QnStoragePluginFactory::instance()->registerStoragePlugin(
        "file", 
        QnFileStorageResource::instance, 
        true
    );
    PluginManager::instance()->loadPlugins(MSSettings::roSettings());

    using namespace std::placeholders;
    for (const auto storagePlugin : 
         PluginManager::instance()->findNxPlugins<nx_spl::StorageFactory>(nx_spl::IID_StorageFactory))
    {
        QnStoragePluginFactory::instance()->registerStoragePlugin(
            storagePlugin->storageType(),
            std::bind(
                &QnThirdPartyStorageResource::instance,
                _1,
                storagePlugin
            ),
            false
        );                    
    }

    QString fileStorageUrl = qApp->applicationDirPath() + lit("/tmp");
    auto rdict = QnResourceStatusDictionary();
    QnStorageResource *fileStorage = QnStoragePluginFactory::instance()->createStorage(fileStorageUrl);
    ASSERT_TRUE(fileStorage);
    fileStorage->setUrl(fileStorageUrl);

    std::vector<QnStorageResource *> storages;
    storages.push_back(fileStorage);

    QnStorageResource *ftpStorage = QnStoragePluginFactory::instance()->createStorage("ftp://anonymous:@127.0.0.1/tmp");
    ASSERT_TRUE(ftpStorage);
    storages.push_back(ftpStorage);

    for (auto storage : storages)
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

        // IODevice
        // write, seeks
        QString fileName = closeDirPath(storage->getUrl()) + lit("test.tmp");
        std::unique_ptr<QIODevice> ioDevice = std::unique_ptr<QIODevice>(
            storage->open(
                fileName.toLatin1().constData(), 
                QIODevice::WriteOnly
            )
        );
        ASSERT_TRUE(ioDevice);

        const size_t dataSize = 10*1024*1024;
        std::vector<char> data(dataSize, 'a');
        ASSERT_TRUE(ioDevice->write(data.data(), dataSize) == dataSize);

        const qint64 seekPos = 10*1024 + 5;
        ASSERT_TRUE(ioDevice->seek(seekPos));

        const char* newData = "bcdefg";
        ASSERT_TRUE(ioDevice->write(newData, std::strlen(newData)) == std::strlen(newData));

        const qint64 seekPos2 = dataSize - 32;
        ASSERT_TRUE(ioDevice->seek(seekPos2));

        ASSERT_TRUE(ioDevice->write(newData, std::strlen(newData)) == std::strlen(newData));
        ioDevice->close();
        ioDevice.reset();

        // IODevice
        // read, seeks
        ioDevice = std::unique_ptr<QIODevice>(
            storage->open(
                fileName.toLatin1().constData(), 
                QIODevice::ReadOnly
            )
        );
        ASSERT_TRUE(ioDevice);
        QByteArray readData = ioDevice->readAll();
        ASSERT_TRUE(readData.size() == dataSize);
        ASSERT_TRUE(
            readData.at(seekPos) == 'b' && 
            readData.at(seekPos + 1) == 'c' &&
            readData.at(seekPos + 2) == 'd' &&
            readData.at(seekPos + 5) == 'g'
        );

        ASSERT_TRUE(
            readData.at(seekPos2) == 'b' && 
            readData.at(seekPos2 + 1) == 'c' &&
            readData.at(seekPos2 + 2) == 'd' &&
            readData.at(seekPos2 + 5) == 'g'
        );

        ASSERT_TRUE(
            readData.at(10) == 'a' && 
            readData.at(dataSize - 1) == 'a'
        );
    }
}
