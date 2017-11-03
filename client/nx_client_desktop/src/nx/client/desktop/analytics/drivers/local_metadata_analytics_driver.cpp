#include "local_metadata_analytics_driver.h"

#include <QtCore/QFileInfo>

#include <core/resource/avi/avi_resource.h>

#include <nx/client/desktop/analytics/camera_metadata_analytics_controller.h>

#include <nx/fusion/model_functions.h>

namespace {

static const QString kMetadataExtension(lit("metadata"));

QString metadataFileName(QString mediaFile)
{
    mediaFile.replace(QFileInfo(mediaFile).suffix(), kMetadataExtension);
    return mediaFile;
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

LocalMetadataAnalyticsDriver::LocalMetadataAnalyticsDriver(QObject* parent):
    base_type(QnResourcePtr(), parent),
    m_resource(QnResourcePtr())
{
}

LocalMetadataAnalyticsDriver::LocalMetadataAnalyticsDriver(const QnResourcePtr& resource,
    QObject* parent)
    :
    base_type(resource, parent),
    m_resource(resource)
{
    if (!loadTrack(m_resource))
        return;

    connect(qnMetadataAnalyticsController, &MetadataAnalyticsController::frameReceived, this,
        [this](const QnResourcePtr& resource, qint64 timestampUs)
        {
            if (resource != m_resource)
                return;

            qnMetadataAnalyticsController->gotMetadata(resource, findMetadata(timestampUs));
        });
}

bool LocalMetadataAnalyticsDriver::loadTrack(const QnResourcePtr& resource)
{
    QFile metadata(metadataFileName(resource->getUrl()));
    metadata.open(QIODevice::ReadOnly);
    m_track = QJson::deserialized<std::vector<nx::common::metadata::DetectionMetadataPacket>>(metadata.readAll());
    metadata.close();

    return !m_track.empty();
}

QRectF LocalMetadataAnalyticsDriver::zoomRectFor(const QnUuid& regionId, qint64 timestampUs) const
{
    auto metadata = findMetadata(timestampUs);
    for (auto region: metadata.objects)
    {
        if (region.objectId == regionId)
            return region.boundingBox;
    }
    return QRectF();
}

bool LocalMetadataAnalyticsDriver::supportsAnalytics(const QnResourcePtr& resource)
{
    const auto file = resource.dynamicCast<QnAviResource>();
    if (!file)
        return false;

    QFileInfo metadata(metadataFileName(resource->getUrl()));
    return metadata.exists();
}

nx::common::metadata::DetectionMetadataPacket LocalMetadataAnalyticsDriver::findMetadata(qint64 timestampUs) const
{
    if (m_track.empty())
        return {};

    auto currentFrame = std::lower_bound(m_track.cbegin(), m_track.cend(), std::chrono::microseconds(timestampUs));
    if (currentFrame == m_track.cend())
        --currentFrame;

    if (currentFrame == m_track.cbegin() && timestampUs < currentFrame->timestampUsec)
        return {};

    if (currentFrame != m_track.cbegin() && timestampUs < currentFrame->timestampUsec)
        --currentFrame;

    NX_EXPECT(currentFrame->timestampUsec <= timestampUs);

    nx::common::metadata::DetectionMetadataPacket metadata;
    metadata.objects = currentFrame->objects;
    return metadata;
}

} // namespace desktop
} // namespace client
} // namespace nx
