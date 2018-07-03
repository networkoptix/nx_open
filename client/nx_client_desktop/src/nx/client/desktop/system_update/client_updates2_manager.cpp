#include "client_updates2_manager.h"

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
namespace updates2 {


using namespace vms::common::p2p::downloader;

namespace {

const int kAutoCheckIntervalMs = 60 * 60 * 1000;  // 1 hour
static const QString kFileName = "update.status";

} // namespace

ClientUpdates2Manager::ClientUpdates2Manager(QnCommonModule* commonModule):
    CommonUpdates2Manager(commonModule), m_downloader(QDir(), commonModule)
{
}

ClientUpdates2Manager::~ClientUpdates2Manager()
{
    m_installer.stopSync();
}

qint64 ClientUpdates2Manager::refreshTimeout() const
{
    //return qnClientModule->settings().checkForUpdateTimeout();
    return kAutoCheckIntervalMs;
}

AbstractDownloader* ClientUpdates2Manager::downloader()
{
    return &m_downloader;
}

update::info::AbstractUpdateRegistryPtr ClientUpdates2Manager::getRemoteRegistry()
{
    auto updateUrl = qnSettings->updateFeedUrl();
    return update::info::checkSync(peerId(), updateUrl);
}

QString ClientUpdates2Manager::filePath() const
{
    //return qnClientModule->settings().dataDir() + QDir::separator() + kFileName;
    return qApp->applicationFilePath();
}

update::installer::detail::AbstractUpdates2Installer* ClientUpdates2Manager::installer()
{
    return &m_installer;
}

} // namespace updates2
} // namespace desktop
} // namespace client
} // namespace nx
