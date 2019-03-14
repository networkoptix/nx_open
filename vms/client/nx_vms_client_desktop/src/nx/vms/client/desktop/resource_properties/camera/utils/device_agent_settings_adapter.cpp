#include "device_agent_settings_adapter.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/api/analytics/settings_response.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <api/server_rest_connection.h>
#include <client_core/connection_context_aware.h>
#include <common/common_module.h>

#include "../redux/camera_settings_dialog_store.h"
#include "../redux/camera_settings_dialog_state.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::common;

class DeviceAgentSettingsAdapter::Private: public QnConnectionContextAware
{
public:
    CameraSettingsDialogStore* store = nullptr;
    QnVirtualCameraResourcePtr camera;
    QnUuid currentEngineId;
    QList<rest::Handle> pendingRefreshRequests;
    QList<rest::Handle> pendingApplyRequests;
};

DeviceAgentSettingsAdapter::DeviceAgentSettingsAdapter(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    d(new Private())
{
    d->store = store;

    d->currentEngineId = d->store->currentAnalyticsEngineId();

    connect(d->store, &CameraSettingsDialogStore::stateChanged, this,
        [this]()
        {
            const auto id = d->store->currentAnalyticsEngineId();
            if (d->currentEngineId == id)
                return;

            d->currentEngineId = id;
            refreshSettings(id);
        });
}

DeviceAgentSettingsAdapter::~DeviceAgentSettingsAdapter()
{
}

void DeviceAgentSettingsAdapter::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (d->camera != camera)
    {
        d->camera = camera;

        if (const auto& server = d->commonModule()->currentServer(); NX_ASSERT(server))
        {
            const auto& connection = server->restConnection();

            // We don't want to refresh requests for previous camera, so cancel them.
            for (const auto& requestId: d->pendingRefreshRequests)
                connection->cancelRequest(requestId);
            d->pendingRefreshRequests.clear();

            // However we want to apply requests to be finished, we just don't need results.
            d->pendingApplyRequests.clear();
        }
    }

    // Refresh settings even if camera is not changed. This happens when the dialog is re-opened.
    refreshSettings(d->currentEngineId);
}

void DeviceAgentSettingsAdapter::refreshSettings(const QnUuid& engineId, bool forceRefresh)
{
    if (!d->camera)
        return;

    if (engineId.isNull())
        return;

    if (!forceRefresh && d->store->state().analytics.settingsValuesByEngineId.contains(engineId))
        return;

    if (!d->pendingApplyRequests.isEmpty())
        return;

    const auto server = d->commonModule()->currentServer();
    if (!NX_ASSERT(server))
        return;

    const auto engine = d->resourcePool()->getResourceById<AnalyticsEngineResource>(engineId);
    if (!NX_ASSERT(engine))
        return;

    const auto handle = server->restConnection()->getDeviceAnalyticsSettings(
        d->camera,
        engine,
        nx::utils::guarded(this,
            [this, engineId](
                bool success, rest::Handle requestId, const nx::vms::api::analytics::SettingsResponse &result)
            {
                if (!d->pendingRefreshRequests.removeOne(requestId))
                    return;

                if (d->pendingRefreshRequests.isEmpty())
                    d->store->setAnalyticsSettingsLoading(false);

                if (!success)
                    return;

                d->store->resetDeviceAgentSettingsValues(engineId, result.values.toVariantMap());
            }),
        thread());

    if (handle <= 0)
        return;

    d->pendingRefreshRequests.append(handle);
    d->store->setAnalyticsSettingsLoading(true);
}

void DeviceAgentSettingsAdapter::applySettings()
{
    if (!d->camera)
        return;

    if (!d->pendingApplyRequests.isEmpty() || !d->pendingRefreshRequests.isEmpty())
        return;

    const auto server = d->commonModule()->currentServer();
    if (!NX_ASSERT(server))
        return;

    const auto& valuesByEngineId = d->store->state().analytics.settingsValuesByEngineId;
    for (auto it = valuesByEngineId.begin(); it != valuesByEngineId.end(); ++it)
    {
        if (!it.value().hasUser())
            continue;

        const auto engine = d->resourcePool()->getResourceById<AnalyticsEngineResource>(it.key());
        if (!NX_ASSERT(engine))
            continue;

        const auto handle = server->restConnection()->setDeviceAnalyticsSettings(
            d->camera,
            engine,
            QJsonObject::fromVariantMap(it->get()),
            nx::utils::guarded(this,
                [this, engineId = it.key()](
                    bool success,
                    rest::Handle requestId,
                    const nx::vms::api::analytics::SettingsResponse& result)
                {
                    if (!d->pendingApplyRequests.removeOne(requestId))
                        return;

                    if (d->pendingApplyRequests.isEmpty())
                        d->store->setAnalyticsSettingsLoading(false);

                    if (!success)
                        return;

                    d->store->resetDeviceAgentSettingsValues(
                        engineId,
                        result.values.toVariantMap());
                }),
            thread());

        if (handle <= 0)
            continue;

        d->pendingApplyRequests.append(handle);
    }

    if (!d->pendingApplyRequests.isEmpty())
        d->store->setAnalyticsSettingsLoading(true);
}

} // namespace nx::vms::client::desktop
