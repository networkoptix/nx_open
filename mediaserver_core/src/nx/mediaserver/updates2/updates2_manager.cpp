#include <nx/update/info/sync_update_checker.h>
#include "detail/update_request_data_factory.h"
#include <nx/api/updates2/updates2_status_data.h>
#include <common/common_module.h>
#include <utils/common/synctime.h>
#include <media_server/media_server_module.h>
#include <media_server/settings.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>


#include "updates2_manager.h"

namespace nx {
namespace mediaserver {
namespace updates2 {

namespace {

static const qint64 kRefreshTimeoutMs = 60 * 60 * 1000;
static const QString kFileName = "update.status";

static QString filePath()
{
    return qnServerModule->settings()->getDataDirectory()
        + QDir::separator()
        + kFileName;
}

static qint64 refreshTimeout()
{
    const auto settingsValue = qnServerModule->roSettings()->value(
        nx_ms_conf::CHECK_FOR_UPDATE_REFRESH_TIMEOUT).toLongLong();
    return settingsValue == 0 ? kRefreshTimeoutMs : settingsValue;
}

} // namespace

Updates2Manager::Updates2Manager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    loadStatusFromFile();
    m_currentStatus.serverId = qnServerModule->commonModule()->moduleGUID();

    using namespace std::placeholders;
    m_timerManager.addNonStopTimer(
        std::bind(&Updates2Manager::checkForUpdateUnsafe, this, _1),
        std::chrono::milliseconds(refreshTimeout()),
        std::chrono::milliseconds(0));
}

api::Updates2StatusData Updates2Manager::status()
{
    QnMutexLocker lock(&m_mutex);
    const detail::Updates2StatusDataEx oldData = m_currentStatus;
    switch (m_currentStatus.status)
    {
        case api::Updates2StatusData::StatusCode::notAvailable:
        case api::Updates2StatusData::StatusCode::error:
        case api::Updates2StatusData::StatusCode::available:
            checkForUpdateUnsafe();
            break;
        case api::Updates2StatusData::StatusCode::downloading:
            updateDownloadingStatus();
            break;
        case api::Updates2StatusData::StatusCode::preparing:
            updatePreparingStatus();
            break;
        case api::Updates2StatusData::StatusCode::readyToInstall:
        case api::Updates2StatusData::StatusCode::checking:
            break;
    }

    if (m_currentStatus != oldData)
        writeStatusToFile();

    return m_currentStatus.base();
}

void Updates2Manager::writeStatusToFile()
{
    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly) || !file.write(QJson::serialized(m_currentStatus)))
    {
        NX_WARNING(this, "Failed to save persistent update status data");
        return;
    }
    m_currentStatus.lastRefreshTime = qnSyncTime->currentMSecsSinceEpoch();
}

void Updates2Manager::loadStatusFromFile()
{
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        NX_VERBOSE(this, "Failed to open persistent update status data");
        return;
    }

    const QByteArray content = file.readAll();
    if (!QJson::deserialize(content, &m_currentStatus))
        NX_VERBOSE(this, "Failed to deserialize persistent update status data");
}


void Updates2Manager::checkForUpdate(utils::TimerId timerId)
{
    if (qnSyncTime->currentMSecsSinceEpoch() - m_currentStatus.lastRefreshTime < refreshTimeout())
        return;

    auto updateUrl= qnServerModule->roSettings()->value(nx_ms_conf::CHECK_FOR_UPDATE_URL).toString();
    updateUrl = updateUrl.isNull() ? update::info::kDefaultUrl : updateUrl;
    const update::info::AbstractUpdateRegistryPtr updateRegistry = update::info::checkSync(updateUrl);
    if (updateRegistry == nullptr)
    {
        m_currentStatus = api::Updates2StatusData(
            commonModule()->moduleGUID(),
            api::Updates2StatusData::StatusCode::error,
            "Failed to get updates data");
        return;
    }

    QnSoftwareVersion version;
    const auto resultCode = updateRegistry->latestUpdate(
        detail::UpdateRequestDataFactory::create(),
        &version);

    if (resultCode != update::info::ResultCode::ok)
    {
        m_currentStatus = api::Updates2StatusData(
            commonModule()->moduleGUID(),
            api::Updates2StatusData::StatusCode::notAvailable,
            "Update is unavailable");
        return;
    }

    m_currentStatus = api::Updates2StatusData(
        commonModule()->moduleGUID(),
        api::Updates2StatusData::StatusCode::available,
        lit("Update is available: %1").arg(version.toString()));
}


void Updates2Manager::updateDownloadingStatus()
{

}

void Updates2Manager::updatePreparingStatus()
{

}



} // namespace updates2
} // namespace mediaserver
} // namespace nx
