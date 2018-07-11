#include "updates_manager.h"

#include <nx/api/updates2/updates2_status_data.h>
#include <nx/update/info/sync_update_checker.h>
#include <nx/update/info/update_registry_factory.h>

#include <common/common_module.h>
#include <utils/common/synctime.h>

#include <client/client_settings.h>
#include <client/client_module.h>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <api/global_settings.h>


namespace nx {
namespace client {
namespace desktop {

using namespace vms::common::p2p::downloader;

namespace {

const int kAutoCheckIntervalMs = 60 * 60 * 1000;  // 1 hour
static const QString kFileName = "update.status";

} // namespace

UpdatesManager::UpdatesManager(QnCommonModule* commonModule):
    CommonUpdates2Manager(commonModule), m_downloader(QDir(), commonModule)
{
}

UpdatesManager::~UpdatesManager()
{
    m_installer.stopSync();
}

qint64 UpdatesManager::refreshTimeout() const
{
    //return qnClientModule->settings().checkForUpdateTimeout();
    return kAutoCheckIntervalMs;
}

AbstractDownloader* UpdatesManager::downloader()
{
    return &m_downloader;
}

update::info::AbstractUpdateRegistryPtr UpdatesManager::getRemoteRegistry()
{
    auto updateUrl = qnSettings->updateFeedUrl();
    return update::info::checkSync(peerId(), updateUrl);
}

QString UpdatesManager::filePath() const
{
    //return qnClientModule->settings().dataDir() + QDir::separator() + kFileName;
    return qApp->applicationFilePath();
}

update::installer::detail::AbstractUpdates2Installer* UpdatesManager::installer()
{
    return &m_installer;
}

} // namespace desktop
} // namespace client
} // namespace nx
