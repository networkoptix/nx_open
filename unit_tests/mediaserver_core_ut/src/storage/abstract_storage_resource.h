#ifndef __ABSTRACT_STORAGE_RESOURCE_TEST__
#define __ABSTRACT_STORAGE_RESOURCE_TEST__

#include <memory>
#include <algorithm>
#include <utility>
#include <cstring>
#include <string>
#include <random>

#include <utils/common/long_runnable.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>
#include <recorder/storage_manager.h>
#include <plugins/plugin_manager.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <plugins/storage/third_party_storage_resource/third_party_storage_resource.h>
#include <media_server/settings.h>
#include <core/resource_management/status_dictionary.h>
#include "utils/common/util.h"

#ifndef _WIN32
#   include <platform/monitoring/global_monitor.h>
#   include <platform/platform_abstraction.h>
#endif

namespace test
{
    const char *const defaultUrl = "ftp://127.0.0.1/tmp";
    struct StorageTestGlobals
    {
        void prepare(const QString &ftpStorageUrl) 
        {
            this->ftpStorageUrl = ftpStorageUrl.isEmpty() ? defaultUrl : ftpStorageUrl;

            runnablePool = std::unique_ptr<QnLongRunnablePool>(
                new QnLongRunnablePool
            );

            resourcePool = std::unique_ptr<QnResourcePool>(
                new QnResourcePool
            );

            storageManager = std::unique_ptr<QnStorageManager>(
                new QnStorageManager
            );

#ifndef _WIN32
            platformAbstraction = std::unique_ptr<QnPlatformAbstraction>(
                new QnPlatformAbstraction
            );
#endif

            QnStoragePluginFactory::instance()->registerStoragePlugin(
                "file", 
                QnFileStorageResource::instance, 
                true
            );
            PluginManager::instance()->loadPlugins(MSSettings::roSettings());

            using namespace std::placeholders;
            for (const auto storagePlugin : 
                    PluginManager::instance()->findNxPlugins<nx_spl::StorageFactory>(
                        nx_spl::IID_StorageFactory
                    ))
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
        }

        ~StorageTestGlobals()
        {
            for (auto storage : storages)
            {
                if (storage)
                    delete storage;
            }
        }

        QString                             ftpStorageUrl;
        std::vector<QnStorageResource *>    storages;
        std::unique_ptr<QnStorageManager>   storageManager;
        std::unique_ptr<QnResourcePool>     resourcePool;
        std::unique_ptr<QnLongRunnablePool> runnablePool;
        QnResourceStatusDictionary          rdict;

#ifndef _WIN32
        std::unique_ptr<QnPlatformAbstraction > platformAbstraction;
#endif
    };
}
#endif
