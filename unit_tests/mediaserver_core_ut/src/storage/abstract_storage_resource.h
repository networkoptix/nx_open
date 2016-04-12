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

namespace test
{
    struct StorageTestGlobals
    {
        void prepare(
            const QString &ftpStorageUrl,
            const QString &smbStorageUrl
        )
        {
            this->ftpStorageUrl = ftpStorageUrl;
            this->smbStorageUrl = smbStorageUrl;

            runnablePool = std::unique_ptr<QnLongRunnablePool>(
                new QnLongRunnablePool
            );

            resourcePool = std::unique_ptr<QnResourcePool>(
                new QnResourcePool
            );

            commonModule = std::unique_ptr<QnCommonModule>(
                new QnCommonModule
            );
            commonModule->setModuleGUID(QnUuid("6F789D28-B675-49D9-AEC0-CEFFC99D674E"));

            storageManager = std::unique_ptr<QnStorageManager>(
                new QnStorageManager(QnServer::StoragePool::Normal)
            );

            fileDeletor = std::unique_ptr<QnFileDeletor>(
                new QnFileDeletor
            );

            pluginManager = std::unique_ptr<PluginManager>(
                new PluginManager
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

            for (const auto storagePlugin : 
                    PluginManager::instance()->findNxPlugins<nx_spl::StorageFactory>(
                        nx_spl::IID_StorageFactory
                    ))
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

        ~StorageTestGlobals()
        {
        }

        QString                             ftpStorageUrl;
        QString                             smbStorageUrl;
        std::unique_ptr<QnFileDeletor>      fileDeletor;
        std::unique_ptr<QnStorageManager>   storageManager;
        std::unique_ptr<QnResourcePool>     resourcePool;
        std::unique_ptr<QnLongRunnablePool> runnablePool;
        std::unique_ptr<QnCommonModule>     commonModule;
        std::unique_ptr<PluginManager>      pluginManager;
        QnResourceStatusDictionary          rdict;

#ifndef _WIN32
        std::unique_ptr<QnPlatformAbstraction > platformAbstraction;
#endif
    };
}
#endif
