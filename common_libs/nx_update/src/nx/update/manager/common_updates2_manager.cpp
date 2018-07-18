#include <nx/api/updates2/updates2_status_data.h>
#include <common/common_module.h>
#include <utils/common/synctime.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <api/global_settings.h>
#include <api/runtime_info_manager.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include "common_updates2_manager.h"


namespace nx {
namespace update {

using namespace vms::common::p2p::downloader;

namespace {

static const QString kFileName = "update.status";

} // namespace

CommonUpdateManager::CommonUpdateManager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

void CommonUpdateManager::connectToSignals()
{
    //connect(
    //    globalSettings(), &QnGlobalSettings::updates2RegistryChanged,
    //    this, &CommonUpdateManager::checkForGlobalDictionaryUpdate, Qt::QueuedConnection);
    //connect(
    //    downloader(), &Downloader::downloadFinished,
    //    this, &CommonUpdateManager::onDownloadFinished, Qt::QueuedConnection);
    //connect(
    //    downloader(), &Downloader::downloadFailed,
    //    this, &CommonUpdateManager::onDownloadFailed, Qt::QueuedConnection);
    //connect(
    //    downloader(), &Downloader::fileDeleted,
    //    this, &CommonUpdateManager::onFileDeleted, Qt::QueuedConnection);
    //connect(
    //    downloader(), &Downloader::fileInformationChanged,
    //    this, &CommonUpdateManager::onFileInformationChanged, Qt::QueuedConnection);
    //connect(
    //    downloader(), &Downloader::fileStatusChanged,
    //    this, &CommonUpdateManager::onFileInformationStatusChanged, Qt::QueuedConnection);
    //connect(
    //    downloader(), &Downloader::chunkDownloadFailed,
    //    this, &CommonUpdateManager::onChunkDownloadFailed, Qt::QueuedConnection);
}


//bool CommonUpdateManager::isClient() const
//{
//    return commonModule()->runtimeInfoManager()->localInfo().data.peer.isClient();
//}

//QnUuid CommonUpdateManager::peerId() const
//{
//    return commonModule()->moduleGUID();
//}

} // namespace update
} // namespace nx
