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

class QnAbstractMediaStreamDataProvider;

namespace nx {
namespace mediaserver {
namespace metadata {

class VideoDataReceptor;

struct ResourceMetadataContext: public QnAbstractDataReceptor
{
public:
    ResourceMetadataContext();
    ~ResourceMetadataContext();

    void setManager(nx::sdk::metadata::AbstractMetadataManager* manager);
    void setHandler(nx::sdk::metadata::AbstractMetadataHandler* handler);
    void setPluginManifest(const nx::api::AnalyticsDriverManifest& manifest);
    void setDataProvider(const QnAbstractMediaStreamDataProvider* provider);
    void setVideoFrameDataReceptor(const QSharedPointer<VideoDataReceptor>& receptor);
    void setMetadataDataReceptor(QnAbstractDataReceptor* receptor);

    nx::sdk::metadata::AbstractMetadataManager* manager() const;
    nx::sdk::metadata::AbstractMetadataHandler* handler() const;
    const QnAbstractMediaStreamDataProvider* dataProvider() const;
    QSharedPointer<VideoDataReceptor> videoFrameDataReceptor() const;
    QnAbstractDataReceptor* metadataDataReceptor() const;
    nx::api::AnalyticsDriverManifest pluginManifest() const;
protected:
    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

private:
    mutable QnMutex m_mutex;
    std::unique_ptr<nx::sdk::metadata::AbstractMetadataManager> m_manager;
    std::unique_ptr<nx::sdk::metadata::AbstractMetadataHandler> m_handler;

    const QnAbstractMediaStreamDataProvider* m_dataProvider = nullptr;
    QSharedPointer<VideoDataReceptor> m_videoFrameDataReceptor;
    QnAbstractDataReceptor* m_metadataReceptor = nullptr;
    nx::api::AnalyticsDriverManifest m_pluginManifest;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
