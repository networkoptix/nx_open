#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <map>
#include <vector>

#include <boost/optional/optional.hpp>

#include <common/common_module_aware.h>
#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <nx/sdk/metadata/plugin.h>
#include <nx/sdk/metadata/camera_manager.h>
#include <nx/api/analytics/driver_manifest.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/mediaserver/metadata/rule_holder.h>
#include <nx/fusion/serialization/json.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include "resource_metadata_context.h"
#include <plugins/plugin_tools.h>
#include <nx/streaming/abstract_data_consumer.h>

class QnAbstractMediaStreamDataProvider;

namespace nx {
namespace mediaserver {
namespace metadata {

class VideoDataReceptor;
using ManagerPtr = nxpt::ScopedRef<nx::sdk::metadata::CameraManager>;
using HandlerPtr = std::unique_ptr<nx::sdk::metadata::MetadataHandler>;

class ManagerContext final: public QnAbstractDataConsumer
{
    using base_type = QnAbstractDataConsumer;
public:
    ManagerContext(
        int queueSize,
        HandlerPtr handler,
        ManagerPtr manager,
        const nx::api::AnalyticsDriverManifest& manifest);
    ~ManagerContext();

    bool isStreamConsumer() const;
    virtual bool processData(const QnAbstractDataPacketPtr& data) override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

    nx::sdk::metadata::CameraManager* manager() { return m_manager.get(); }
    const nx::sdk::metadata::CameraManager* manager() const { return m_manager.get(); }

    nx::sdk::metadata::MetadataHandler* handler() const { return m_handler.get(); }
    const nx::api::AnalyticsDriverManifest& manifest() const { return m_manifest; }

private:
    HandlerPtr m_handler;
    ManagerPtr m_manager;
    nx::api::AnalyticsDriverManifest m_manifest;
};

using ManagerList = std::vector<std::unique_ptr<ManagerContext>>;

/**
 * Private class for internal use inside ManagerPool only
 */
class ResourceMetadataContext: public QnAbstractDataReceptor
{
public:
    ResourceMetadataContext();
    ~ResourceMetadataContext();
    friend class ManagerPool;
private:
    /**
     * Register plugin manager. This function takes ownership for the manager and handler
     */
    void addManager(
        ManagerPtr manager,
        HandlerPtr handler,
        const nx::api::AnalyticsDriverManifest& manifest);
    void clearManagers();
    void setVideoDataReceptor(const QSharedPointer<VideoDataReceptor>& receptor);
    void setMetadataDataReceptor(QWeakPointer<QnAbstractDataReceptor> receptor);

    ManagerList& managers();
    QSharedPointer<VideoDataReceptor> videoDataReceptor() const;
    QWeakPointer<QnAbstractDataReceptor> metadataDataReceptor() const;

    void setManagersInitialized(bool value);
    bool isManagerInitialized() const;
protected:
    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

private:
    ManagerList m_managers;

    QSharedPointer<VideoDataReceptor> m_videoDataReceptor;
    QWeakPointer<QnAbstractDataReceptor> m_metadataReceptor;
    nx::api::AnalyticsDriverManifest m_pluginManifest;
    bool m_isManagerInitialized = false;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
