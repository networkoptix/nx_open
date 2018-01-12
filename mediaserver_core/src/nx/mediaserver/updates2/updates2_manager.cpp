#include "detail/update_request_data_factory.h"
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

static bool isNewRegistryBetter(
    const update::info::AbstractUpdateRegistryPtr& oldRegistry,
    const update::info::AbstractUpdateRegistryPtr& newRegistry)
{
    if (!oldRegistry)
        return true;
    QnSoftwareVersion newRegistryServerVersion;
    if (newRegistry->latestUpdate(
        detail::UpdateRequestDataFactory::create(),
        &newRegistryServerVersion) != update::info::ResultCode::ok)
    {
        return false;
    }
    QnSoftwareVersion oldRegistryServerVersion;
    if (oldRegistry->latestUpdate(
        detail::UpdateRequestDataFactory::create(),
        &oldRegistryServerVersion) != update::info::ResultCode::ok
        || newRegistryServerVersion > oldRegistryServerVersion)
    {
        return true;
    }
    return false;
}

} // namespace

Updates2Manager::Updates2Manager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    loadStatusFromFile();
}

void Updates2Manager::startTimers()
{
    checkForGlobalDictionaryUpdate();
    using namespace std::placeholders;
    m_timerManager.addNonStopTimer(
        std::bind(&Updates2Manager::checkForRemoteUpdate, this, _1),
        std::chrono::milliseconds(refreshTimeout()),
        std::chrono::milliseconds(0));
}

api::Updates2StatusData Updates2Manager::status()
{
    QnMutexLocker lock(&m_mutex);
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

void Updates2Manager::checkForGlobalDictionaryUpdate()
{
    auto globalRegistry = update::info::UpdateRegistryFactory::create();
    bool deserializeResult = globalRegistry->fromByteArray(globalSettings()->updates2Registry());
    if (!deserializeResult)
        return;
    swapRegistries(std::move(globalRegistry));
    refreshStatusAfterCheck();
}

void Updates2Manager::checkForRemoteUpdate(utils::TimerId /*timerId*/)
{
    {
        QnMutexLocker lock(&m_mutex);
        switch (m_currentStatus.status)
        {
            case api::Updates2StatusData::StatusCode::notAvailable:
            case api::Updates2StatusData::StatusCode::error:
            case api::Updates2StatusData::StatusCode::available:
                m_currentStatus = api::Updates2StatusData(
                    commonModule()->moduleGUID(),
                    api::Updates2StatusData::StatusCode::checking,
                    "Checking for update");
                break;
            default:
                return;
        }
    }

    auto updateUrl= qnServerModule->roSettings()->value(
        nx_ms_conf::CHECK_FOR_UPDATE_URL).toString();
    updateUrl = updateUrl.isNull() ? update::info::kDefaultUrl : updateUrl;
    update::info::AbstractUpdateRegistryPtr updateRegistry = update::info::checkSync(updateUrl);

    if (updateRegistry)
    {
        auto globalRegistry = update::info::UpdateRegistryFactory::create();
        if (!globalRegistry->fromByteArray(globalSettings()->updates2Registry())
            || isNewRegistryBetter(globalRegistry, updateRegistry))
        {
            globalSettings()->setUpdates2Registry(updateRegistry->toByteArray());
            globalSettings()->synchronizeNow();
        }
        swapRegistries(std::move(updateRegistry));
    }

    refreshStatusAfterCheck();
}


void Updates2Manager::swapRegistries(update::info::AbstractUpdateRegistryPtr otherRegistry)
{
    QnMutexLocker lock(&m_mutex);
    if (isNewRegistryBetter(m_updateRegistry, otherRegistry))
        m_updateRegistry = std::move(otherRegistry);
}

void Updates2Manager::refreshStatusAfterCheck()
{
    QnMutexLocker lock(&m_mutex);
    switch (m_currentStatus.status)
    {
        case api::Updates2StatusData::StatusCode::notAvailable:
        case api::Updates2StatusData::StatusCode::error:
        case api::Updates2StatusData::StatusCode::available:
        case api::Updates2StatusData::StatusCode::checking:
        {
            QnSoftwareVersion version;
            if (!m_updateRegistry)
            {
                m_currentStatus = detail::Updates2StatusDataEx(
                    qnSyncTime->currentMSecsSinceEpoch(),
                    commonModule()->moduleGUID(),
                    api::Updates2StatusData::StatusCode::error,
                    "Failed to get updates data");
            }
            else
            {
                if (m_updateRegistry->latestUpdate(
                    detail::UpdateRequestDataFactory::create(),
                    &version) != update::info::ResultCode::ok)
                {
                    m_currentStatus = detail::Updates2StatusDataEx(
                        qnSyncTime->currentMSecsSinceEpoch(),
                        commonModule()->moduleGUID(),
                        api::Updates2StatusData::StatusCode::notAvailable,
                        "Update is unavailable");
                }
                else
                {
                    m_currentStatus = detail::Updates2StatusDataEx(
                        qnSyncTime->currentMSecsSinceEpoch(),
                        commonModule()->moduleGUID(),
                        api::Updates2StatusData::StatusCode::available,
                        lit("Update is available: %1").arg(version.toString()));
                }
            }

            writeStatusToFile();
            break;
        }
        default:
            return;
    }
}

} // namespace updates2
} // namespace mediaserver
} // namespace nx
