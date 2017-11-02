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

struct ManagerContext
{
    HandlerPtr handler;
    ManagerPtr manager;
    nx::api::AnalyticsDriverManifest manifest;
};

using ManagerList = std::vector<ManagerContext>;

class ResourceMetadataContext: public QnAbstractDataReceptor
{
public:
    ResourceMetadataContext();
    ~ResourceMetadataContext();

    /**
     * Register plugin manager. This function takes ownership for the manager and handler
     */
    void addManager(
        ManagerPtr manager,
        HandlerPtr handler,
        const nx::api::AnalyticsDriverManifest& manifest);
    void clearManagers();
    void setDataProvider(const QnAbstractMediaStreamDataProvider* provider);
    void setVideoFrameDataReceptor(const QSharedPointer<VideoDataReceptor>& receptor);
    void setMetadataDataReceptor(QnAbstractDataReceptor* receptor);

    ManagerList& managers();
    const QnAbstractMediaStreamDataProvider* dataProvider() const;
    QSharedPointer<VideoDataReceptor> videoFrameDataReceptor() const;
    QnAbstractDataReceptor* metadataDataReceptor() const;

    void setManagersInitialized(bool value);
    bool isManagerInitialized() const;
protected:
    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

private:
    mutable QnMutex m_mutex;
    ManagerList m_managers;

    const QnAbstractMediaStreamDataProvider* m_dataProvider = nullptr;
    QSharedPointer<VideoDataReceptor> m_videoFrameDataReceptor;
    QnAbstractDataReceptor* m_metadataReceptor = nullptr;
    nx::api::AnalyticsDriverManifest m_pluginManifest;
    bool m_isManagerInitialized = false;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
