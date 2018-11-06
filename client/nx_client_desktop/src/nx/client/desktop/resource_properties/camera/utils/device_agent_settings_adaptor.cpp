#include "device_agent_settings_adaptor.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <api/server_rest_connection.h>
#include <client_core/connection_context_aware.h>
#include <common/common_module.h>

#include "../redux/camera_settings_dialog_store.h"
#include "../redux/camera_settings_dialog_state.h"

namespace nx::client::desktop {

using namespace nx::vms::common;

class DeviceAgentSettingsAdaptor::Private: public QnConnectionContextAware
{
public:
    CameraSettingsDialogStore* store = nullptr;
    QnVirtualCameraResourcePtr camera;
    QList<rest::Handle> pendingRefreshRequests;
    QList<rest::Handle> pendingApplyRequests;
};

DeviceAgentSettingsAdaptor::DeviceAgentSettingsAdaptor(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    d(new Private())
{
    d->store = store;
}

DeviceAgentSettingsAdaptor::~DeviceAgentSettingsAdaptor()
{
}

void DeviceAgentSettingsAdaptor::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (d->camera == camera)
        return;

    d->camera = camera;

    if (const auto& server = d->commonModule()->currentServer(); NX_ASSERT(server))
    {
        const auto& connection = server->restConnection();

        // We don't want refresh requests for previous camera, so cancel them.
        for (const auto& requestId: d->pendingRefreshRequests)
            connection->cancelRequest(requestId);
        d->pendingRefreshRequests.clear();

        // However we want apply requests to be finished, we just don't need results.
        d->pendingApplyRequests.clear();
    }
}

void DeviceAgentSettingsAdaptor::refreshSettings(const QnUuid& engineId, bool forceRefresh)
{
    if (!d->camera)
        return;

    if (!NX_ASSERT(!engineId.isNull()))
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
                bool success, rest::Handle requestId, const QJsonObject& result)
            {
                if (!d->pendingRefreshRequests.removeOne(requestId))
                    return;

                if (d->pendingRefreshRequests.isEmpty())
                    d->store->setAnalyticsSettingsLoading(false);

                if (!success)
                    return;

                d->store->resetDeviceAgentSettingsValues(engineId, result.toVariantMap());
            }),
        thread());

    if (handle <= 0)
        return;

    d->pendingRefreshRequests.append(handle);
    d->store->setAnalyticsSettingsLoading(true);
}

void DeviceAgentSettingsAdaptor::applySettings()
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
                    bool success, rest::Handle requestId, const QJsonObject& result)
                {
                    if (!d->pendingApplyRequests.removeOne(requestId))
                        return;

                    if (d->pendingApplyRequests.isEmpty())
                        d->store->setAnalyticsSettingsLoading(false);

                    if (!success)
                        return;

                    d->store->resetDeviceAgentSettingsValues(engineId, result.toVariantMap());
                }),
            thread());

        if (handle <= 0)
            continue;

        d->pendingApplyRequests.append(handle);
    }

    if (!d->pendingApplyRequests.isEmpty())
        d->store->setAnalyticsSettingsLoading(true);
}

} // namespace nx::client::desktop
