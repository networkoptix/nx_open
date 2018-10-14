#include "device_analytics_context.h"

#include <core/resource/camera_resource.h>

#include <nx/mediaserver/resource/analytics_plugin_resource.h>
#include <nx/mediaserver/resource/analytics_engine_resource.h>
#include <nx/mediaserver/analytics/device_analytics_binding.h>

#include <nx/utils/log/log.h>

namespace nx::mediaserver::analytics {

DeviceAnalyticsContext::DeviceAnalyticsContext(
    QnMediaServerModule* serverModule,
    const QnVirtualCameraResourcePtr& device)
    :
    base_type(serverModule),
    m_device(device)
{
    subscribeToDeviceChanges();
}

bool DeviceAnalyticsContext::isEngineAlreadyBound(
    const QnUuid& engineId) const
{
    return m_bindings.find(engineId) != m_bindings.cend();
}

bool DeviceAnalyticsContext::isEngineAlreadyBound(
    const nx::vms::common::AnalyticsEngineResourcePtr& engine) const
{
    return isEngineAlreadyBound(engine->getId());
}

void DeviceAnalyticsContext::setEnabledAnalyticsEngines(
    const nx::vms::common::AnalyticsEngineResourceList& engines)
{
    std::set<QnUuid> engineIds;
    for (const auto& engine: engines)
        engineIds.insert(engine->getId());

    for (auto itr = m_bindings.begin(); itr != m_bindings.cend();)
    {
        if (engineIds.find(itr->first) == engineIds.cend())
            itr = m_bindings.erase(itr);
        else
            ++itr;
    }

    for (const auto& engine: engines)
    {
        if (isEngineAlreadyBound(engine))
            continue;

        auto serverEngine = engine.dynamicCast<resource::AnalyticsEngineResource>();
        auto binding = std::make_shared<DeviceAnalyticsBinding>(m_device, serverEngine);
        m_bindings.emplace(engine->getId(), binding);
    }
}

void DeviceAnalyticsContext::addEngine(const nx::vms::common::AnalyticsEngineResourcePtr& engine)
{
    auto serverEngine = engine.dynamicCast<resource::AnalyticsEngineResource>();
    if (!serverEngine)
    {
        NX_WARNING(this, lm("Engine is not an instance of server analytics engine, skipping."));
        return;
    }

    auto binding = std::make_shared<DeviceAnalyticsBinding>(m_device, serverEngine);
    m_bindings.emplace(engine->getId(), binding);
}

void DeviceAnalyticsContext::removeEngine(const nx::vms::common::AnalyticsEngineResourcePtr& engine)
{
    m_bindings.erase(engine->getId());
}

void DeviceAnalyticsContext::at_deviceUpdated(const QnResourcePtr& resource)
{
    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!device)
    {
        NX_ASSERT(device, "Invalid device");
        return;
    }

    for (auto& entry: m_bindings)
    {
        auto engineId = entry.first;
        auto binding = entry.second;
        binding->restartAnalytics(m_device->deviceAgentSettingsValues(engineId));
    }
}

void DeviceAnalyticsContext::at_devicePropertyChanged(
    const QnResourcePtr& resource,
    const QString& propertyName)
{
    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!device)
    {
        NX_ASSERT(false, "Invalid device");
        return;
    }

    if (propertyName == Qn::CAMERA_CREDENTIALS_PARAM_NAME)
    {
        NX_DEBUG(
            this,
            lm("Credentials have been changed for resource %1 (%2)")
                .args(device->getName(), device->getId()));

        at_deviceUpdated(device);
    }
}

void DeviceAnalyticsContext::subscribeToDeviceChanges()
{
    connect(
        m_device, &QnResource::statusChanged,
        this, &DeviceAnalyticsContext::at_deviceUpdated);

    connect(
        m_device, &QnResource::urlChanged,
        this, &DeviceAnalyticsContext::at_deviceUpdated);

    connect(
        m_device, &QnVirtualCameraResource::logicalIdChanged,
        this, &DeviceAnalyticsContext::at_deviceUpdated);

    connect(
        m_device, &QnResource::propertyChanged,
        this, &DeviceAnalyticsContext::at_devicePropertyChanged);
}


} // namespace nx::mediaserver::analytics
