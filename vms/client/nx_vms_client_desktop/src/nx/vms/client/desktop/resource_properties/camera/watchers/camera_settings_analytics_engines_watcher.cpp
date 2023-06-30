// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_analytics_engines_watcher.h"

#include <algorithm>

#include <client/client_message_processor.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/client/core/analytics/analytics_data_helper.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/common/saas/saas_service_manager.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

class CameraSettingsAnalyticsEnginesWatcher::Private:
    public QObject,
    public SystemContextAware
{
public:
    Private(SystemContext* systemContext, CameraSettingsDialogStore* store);

    QList<core::AnalyticsEngineInfo> engineInfoList() const;

    void setCamera(const QnVirtualCameraResourcePtr& camera);
    void updateStore();
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_engineNameChanged(const QnResourcePtr& resource);
    void at_engineManifestChanged(const AnalyticsEngineResourcePtr& engine);
    void at_initialResourcesReceived();
    void at_connectionClosed();

    nx::vms::api::StreamIndex analyzedStreamIndex(const QnUuid& engineId) const;

public:
    const QPointer<CameraSettingsDialogStore> store;
    QnVirtualCameraResourcePtr camera;
    QHash<QnUuid, core::AnalyticsEngineInfo> engines;
    bool m_online = false;
};

CameraSettingsAnalyticsEnginesWatcher::Private::Private(
    SystemContext* systemContext,
    CameraSettingsDialogStore* store)
    :
    SystemContextAware(systemContext),
    store(store)
{
    const auto messageProcessor = qnClientMessageProcessor;

    if (!NX_ASSERT(messageProcessor))
        return;

    connect(messageProcessor, &QnCommonMessageProcessor::initialResourcesReceived,
        this, &Private::at_initialResourcesReceived);
    connect(messageProcessor, &QnCommonMessageProcessor::connectionClosed,
        this, &Private::at_connectionClosed);

    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, &Private::at_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, &Private::at_resourceRemoved);

    if (messageProcessor->connectionStatus()->state() == QnConnectionState::Ready)
        at_initialResourcesReceived();
}

QList<core::AnalyticsEngineInfo> CameraSettingsAnalyticsEnginesWatcher::Private::engineInfoList() const
{
    if (!camera)
        return {};

    auto enginesList = engines.values();
    const auto compatibleEngines = camera->compatibleAnalyticsEngines();

    const auto isIncompatibleEngine =
        [compatibleEngines](const core::AnalyticsEngineInfo& engineInfo)
        {
            return !compatibleEngines.contains(engineInfo.id);
        };

    enginesList.erase(
        std::remove_if(enginesList.begin(), enginesList.end(), isIncompatibleEngine),
        enginesList.end());

    std::sort(enginesList.begin(), enginesList.end(),
        [](const auto& left, const auto& right) { return left.name < right.name; });

    return enginesList;
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_initialResourcesReceived()
{
    // In certain rare cases this function can be called while m_online is true.
    // This happens in scenarions like this:
    //   - connection opened
    //   - connection closed
    //   - connection opened
    //   - initial resources received
    //   - initial resources received
    m_online = true;

    engines.clear();

    for (const auto& engine: resourcePool()->getResources<AnalyticsEngineResource>())
        at_resourceAdded(engine);
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_connectionClosed()
{
    // Do not assert here as we can get connection closed before resources are received.
    m_online = false;
}

void CameraSettingsAnalyticsEnginesWatcher::Private::setCamera(
    const QnVirtualCameraResourcePtr& newCamera)
{
    if (camera == newCamera)
        return;

    if (camera)
        camera->disconnect(this);

    camera = newCamera;

    if (camera)
    {
        connect(camera.get(), &QnResource::parentIdChanged, this, &Private::updateStore);
        connect(camera.get(), &QnResource::propertyChanged, this,
            [this](const QnResourcePtr& resource, const QString& key)
            {
                if (resource == camera
                    && (key == QnVirtualCameraResource::kAnalyzedStreamIndexes
                        || key == QnVirtualCameraResource::kDeviceAgentManifestsProperty))
                {
                    updateStore();
                }
            });
    }
}

void CameraSettingsAnalyticsEnginesWatcher::Private::updateStore()
{
    if (store)
    {
        const auto enginesList = engineInfoList();
        store->setAnalyticsEngines(enginesList);

        for (const auto& engine: enginesList)
        {
            store->setAnalyticsStreamIndex(
                engine.id, analyzedStreamIndex(engine.id), ModificationSource::remote);
        }
    }
}

nx::vms::api::StreamIndex CameraSettingsAnalyticsEnginesWatcher::Private::analyzedStreamIndex(
    const QnUuid& engineId) const
{
    if (!camera)
        return nx::vms::api::StreamIndex::undefined;

    const auto manifest = camera->deviceAgentManifest(engineId);
    const auto disableStreamSelection = manifest && manifest->capabilities
        .testFlag(nx::vms::api::analytics::DeviceAgentManifest::disableStreamSelection);

    return disableStreamSelection
        ? nx::vms::api::StreamIndex::undefined
        : camera->analyzedStreamIndex(engineId);
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_resourceAdded(
    const QnResourcePtr& resource)
{
    if (!m_online)
        return;

    if (const auto& engine = resource.objectCast<AnalyticsEngineResource>())
    {
        const auto info = engineInfoFromResource(engine, core::SettingsModelSource::manifest);
        if (info.id.isNull())
            return;

        engines[info.id] = info;

        connect(engine.get(), &QnResource::nameChanged,
            this, &Private::at_engineNameChanged);
        connect(engine.get(), &AnalyticsEngineResource::manifestChanged,
            this, &Private::at_engineManifestChanged);

        updateStore();
    }
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_resourceRemoved(
    const QnResourcePtr& resource)
{
    if (const auto& engine = resource.objectCast<AnalyticsEngineResource>())
    {
        engine->disconnect(this);
        if (engines.remove(engine->getId()) > 0)
            updateStore();
    }
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_engineNameChanged(
    const QnResourcePtr& resource)
{
    const auto it = engines.find(resource->getId());
    if (it == engines.end())
        return;

    it->name = resource->getName();
    updateStore();
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_engineManifestChanged(
    const AnalyticsEngineResourcePtr& engine)
{
    if (const auto it = engines.find(engine->getId()); it != engines.end())
    {
        it->settingsModel = engine->manifest().deviceAgentSettingsModel;
        it->isDeviceDependent = engine->isDeviceDependent();
        updateStore();
    }
}

CameraSettingsAnalyticsEnginesWatcher::CameraSettingsAnalyticsEnginesWatcher(
    CameraSettingsDialogStore* store, QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(systemContext(), store))
{
}

CameraSettingsAnalyticsEnginesWatcher::~CameraSettingsAnalyticsEnginesWatcher()
{
}

QnVirtualCameraResourcePtr CameraSettingsAnalyticsEnginesWatcher::camera() const
{
    return d->camera;
}

void CameraSettingsAnalyticsEnginesWatcher::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    d->setCamera(camera);
}

QList<core::AnalyticsEngineInfo> CameraSettingsAnalyticsEnginesWatcher::engineInfoList() const
{
    return d->engineInfoList();
}

nx::vms::api::StreamIndex CameraSettingsAnalyticsEnginesWatcher::analyzedStreamIndex(
    const QnUuid& engineId) const
{
    return d->analyzedStreamIndex(engineId);
}

} // namespace nx::vms::client::desktop
