#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <map>
#include <vector>

#include <boost/optional/optional.hpp>

#include <common/common_module_aware.h>
#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/device_agent.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/mediaserver/analytics/rule_holder.h>
#include <nx/fusion/serialization/json.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include "resource_analytics_context.h"
#include <plugins/plugin_tools.h>
#include <nx/streaming/abstract_data_consumer.h>

class QnAbstractMediaStreamDataProvider;

namespace nx {
namespace mediaserver {
namespace analytics {

class VideoDataReceptor;
using DeviceAgentPtr = nxpt::ScopedRef<nx::sdk::analytics::DeviceAgent>;
using MetadataHandlerPtr = std::unique_ptr<nx::sdk::analytics::MetadataHandler>;

class DeviceAgentContext final: public QnAbstractDataConsumer
{
    using base_type = QnAbstractDataConsumer;

public:
    DeviceAgentContext(
        int queueSize,
        MetadataHandlerPtr metadataHandler,
        DeviceAgentPtr deviceAgent,
        const nx::vms::api::analytics::EngineManifest& engineManifest);

    ~DeviceAgentContext();

    bool isStreamConsumer() const;
    virtual bool processData(const QnAbstractDataPacketPtr& data) override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

    nx::sdk::analytics::DeviceAgent* deviceAgent() { return m_deviceAgent.get(); }
    const nx::sdk::analytics::DeviceAgent* deviceAgent() const { return m_deviceAgent.get(); }

    nx::sdk::analytics::MetadataHandler* metadataHandler() const
    {
        return m_metadataHandler.get();
    }

    const nx::vms::api::analytics::EngineManifest& engineManifest() const
    {
        return m_engineManifest;
    }

private:
    MetadataHandlerPtr m_metadataHandler;
    DeviceAgentPtr m_deviceAgent;
    nx::vms::api::analytics::EngineManifest m_engineManifest;
};

using DeviceAgentContextList = std::vector<std::unique_ptr<DeviceAgentContext>>;

/**
 * Private class for internal use inside analytics Manager only.
 */
class ResourceAnalyticsContext: public QnAbstractDataReceptor
{
public:
    ResourceAnalyticsContext();
    ~ResourceAnalyticsContext();
    friend class Manager;
    
private:
    /**
     * Registers a DeviceAgent. Takes ownership of deviceAgent and metadataHandler.
     */
    void addDeviceAgent(
        DeviceAgentPtr deviceAgent,
        MetadataHandlerPtr metadataHandler,
        const nx::vms::api::analytics::EngineManifest& engineManifest);

    void clearDeviceAgentContexts();
    void setVideoDataReceptor(const QSharedPointer<VideoDataReceptor>& receptor);
    void setMetadataDataReceptor(QWeakPointer<QnAbstractDataReceptor> receptor);

    DeviceAgentContextList& deviceAgentContexts();
    QSharedPointer<VideoDataReceptor> videoDataReceptor() const;
    QWeakPointer<QnAbstractDataReceptor> metadataDataReceptor() const;

    void setDeviceAgentContextsInitialized(bool value);
    bool areDeviceAgentContextsInitialized() const;
protected:
    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

private:
    DeviceAgentContextList m_deviceAgentContexts;

    QSharedPointer<VideoDataReceptor> m_videoDataReceptor;
    QWeakPointer<QnAbstractDataReceptor> m_metadataReceptor;
    nx::vms::api::analytics::EngineManifest m_engineManifest;
    bool m_deviceAgentContextsInitialized = false;
};

} // namespace analytics
} // namespace mediaserver
} // namespace nx
