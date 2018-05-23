#include "server_updates2_manager.h"
#include <nx/api/updates2/updates2_status_data.h>
#include <nx/update/info/sync_update_checker.h>
#include <nx/update/info/update_registry_factory.h>
#include <common/common_module.h>
#include <utils/common/synctime.h>
#include <media_server/media_server_module.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <media_server/settings.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <api/global_settings.h>
#include <nx/vms/common/p2p/downloader/downloader.h>


namespace nx {
namespace mediaserver {
namespace updates2 {

using namespace vms::common::p2p::downloader;

namespace {

static const qint64 kRefreshTimeoutMs = 24 * 60 * 60 * 1000; //< 1 day
static const QString kFileName = "update.status";

} // namespace

ServerUpdates2Manager::ServerUpdates2Manager(QnCommonModule* commonModule):
    CommonUpdates2Manager(commonModule)
{
}

ServerUpdates2Manager::~ServerUpdates2Manager()
{
    m_installer.stopSync();
}

qint64 ServerUpdates2Manager::refreshTimeout() const
{
    const auto settingsValue = qnServerModule->roSettings()->value(
        nx_ms_conf::CHECK_FOR_UPDATE_TIMEOUT).toLongLong();
    return settingsValue == 0 ? kRefreshTimeoutMs : settingsValue;
}

AbstractDownloader* ServerUpdates2Manager::downloader()
{
    return qnServerModule->findInstance<vms::common::p2p::downloader::Downloader>();
}

update::info::AbstractUpdateRegistryPtr ServerUpdates2Manager::getRemoteRegistry()
{
    auto updateUrl =
        qnServerModule->roSettings()->value(nx_ms_conf::CHECK_FOR_UPDATE_URL).toString();
    updateUrl = updateUrl.isNull() ? update::info::kDefaultUrl : updateUrl;

    return update::info::checkSync(peerId(), updateUrl);
}

QString ServerUpdates2Manager::filePath() const
{
    return qnServerModule->settings()->getDataDirectory() + QDir::separator() + kFileName;
}

update::installer::detail::AbstractUpdates2Installer* ServerUpdates2Manager::installer()
{
    return &m_installer;
}

} // namespace updates2
} // namespace mediaserver
} // namespace nx
