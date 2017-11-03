#include "local_metadata_analytics_driver.h"

#include <QtCore/QFileInfo>

#include <plugins/resource/avi/avi_resource.h>

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
    m_track = QJson::deserialized<std::vector<QnObjectDetectionMetadataTrack>>(metadata.readAll());
    metadata.close();

    return !m_track.empty();
}

QRectF LocalMetadataAnalyticsDriver::zoomRectFor(const QnUuid& regionId, qint64 timestampUs) const
{
    QnObjectDetectionMetadata metadata = findMetadata(timestampUs);
    for (auto region: metadata.detectedObjects)
    {
        if (region.objectId == regionId)
            return region.boundingBox.toRectF();
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

QnObjectDetectionMetadata LocalMetadataAnalyticsDriver::findMetadata(qint64 timestampUs) const
{
    if (m_track.empty())
        return {};

    const auto timestampMs = timestampUs / 1000;

    auto currentFrame = std::lower_bound(m_track.cbegin(), m_track.cend(), timestampMs);
    if (currentFrame == m_track.cend())
        --currentFrame;

    if (currentFrame == m_track.cbegin() && timestampMs < *currentFrame)
        return {};

    if (currentFrame != m_track.cbegin() && timestampMs < *currentFrame)
        --currentFrame;

    NX_EXPECT(currentFrame->timestampMs <= timestampMs);

    QnObjectDetectionMetadata metadata;
    metadata.detectedObjects = currentFrame->objects;
    return metadata;
}

} // namespace desktop
} // namespace client
} // namespace nx
