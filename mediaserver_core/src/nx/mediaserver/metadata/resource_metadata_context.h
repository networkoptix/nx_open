#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <map>
#include <vector>

#include <boost/optional/optional.hpp>

#include <common/common_module_aware.h>
#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>
#include <nx/api/analytics/driver_manifest.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/mediaserver/metadata/rule_holder.h>
#include <nx/fusion/serialization/json.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include "resource_metadata_context.h"
#include <plugins/plugin_tools.h>

class QnAbstractMediaStreamDataProvider;

namespace nx {
namespace mediaserver {
namespace metadata {

class VideoDataReceptor;
using ManagerPtr = nxpt::ScopedRef<nx::sdk::metadata::AbstractMetadataManager>;
using HandlerPtr = std::unique_ptr<nx::sdk::metadata::AbstractMetadataHandler>;

struct ManagerContext final
{
    HandlerPtr handler;
    ManagerPtr manager;
    nx::api::AnalyticsDriverManifest manifest;
    ManagerContext() = default;
    ManagerContext(ManagerContext&& other) = default;
    ~ManagerContext();
};

using ManagerList = std::vector<ManagerContext>;

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
    void setVideoFrameDataReceptor(const QSharedPointer<VideoDataReceptor>& receptor);
    void setMetadataDataReceptor(QWeakPointer<QnAbstractDataReceptor> receptor);

    ManagerList& managers();
    QSharedPointer<VideoDataReceptor> videoFrameDataReceptor() const;
    QWeakPointer<QnAbstractDataReceptor> metadataDataReceptor() const;

    void setManagersInitialized(bool value);
    bool isManagerInitialized() const;
protected:
    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

private:
    ManagerList m_managers;

    QSharedPointer<VideoDataReceptor> m_videoFrameDataReceptor;
    QWeakPointer<QnAbstractDataReceptor> m_metadataReceptor;
    nx::api::AnalyticsDriverManifest m_pluginManifest;
    bool m_isManagerInitialized = false;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
