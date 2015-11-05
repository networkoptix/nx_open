#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>

#include <plugins/resource/server_archive/server_archive_delegate.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource_management/resource_pool.h>
#include <recorder/storage_manager.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <core/resource_management/status_dictionary.h>
#include <common/common_module.h>
#include <core/resource_management/resource_properties.h>

#include <functional>
#include <thread>
#include <chrono>

bool recursiveClean(const QString &path) {
    QDir dir(path);
    QFileInfoList entryList = dir.entryInfoList(
            QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot, 
            QDir::DirsFirst); 

    for (auto &entry : entryList) {
        auto tmp = entry.absoluteFilePath();
        if (entry.isDir()) {
             if (!recursiveClean(entry.absoluteFilePath()))
                return false;
        } 
        else if (entry.isFile()) {
            QFile f(entry.absoluteFilePath());
            f.setPermissions(QFile::ReadOther | QFile::WriteOther);
            auto result = f.remove();
            if (!result)
                return false;
        }
    }
    dir.rmpath(path);
    return true;
};

TEST(ServerArchiveDelegate_playback_test, Main) {
    std::unique_ptr<QnCommonModule> commonModule;
    if (!qnCommon) {
        commonModule = std::unique_ptr<QnCommonModule>(new QnCommonModule);
    }
    commonModule->setModuleGUID("{A680980C-70D1-4545-A5E5-72D89E33648B}");

    std::unique_ptr<QnStorageManager> storageManager;
    if (!qnNormalStorageMan) {
        storageManager = std::unique_ptr<QnStorageManager>(new QnStorageManager(QnServer::StoragePool::Normal));
    }
    storageManager->stopAsyncTasks();

    std::unique_ptr<QnResourceStatusDictionary> statusDictionary;
    if (!qnStatusDictionary) {
        statusDictionary = std::unique_ptr<QnResourceStatusDictionary>(new QnResourceStatusDictionary);
    }

    std::unique_ptr<QnResourcePropertyDictionary> propDictionary;
    if (!propertyDictionary) {
        propDictionary = std::unique_ptr<QnResourcePropertyDictionary>(new QnResourcePropertyDictionary);
    }

    std::unique_ptr<QnStorageDbPool> dbPool;
    if (!qnStorageDbPool) {
        dbPool = std::unique_ptr<QnStorageDbPool>(new QnStorageDbPool);
    }

    //storage1->setStatus(Qn::Online, true);
    //storage2->setStatus(Qn::Online, true);
    //storage3->setStatus(Qn::Online, true);
}