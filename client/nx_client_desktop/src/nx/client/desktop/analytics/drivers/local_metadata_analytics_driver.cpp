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

LocalMetadataAnalyticsDriver::LocalMetadataAnalyticsDriver(const QnResourcePtr& resource,
    QObject* parent)
    :
    base_type(resource, parent),
    m_resource(resource)
{
    QFile metadata(metadataFileName(resource->getUrl()));
    metadata.open(QIODevice::ReadOnly);
    m_track = QJson::deserialized<std::vector<nx::common::metadata::DetectionMetadataPacket>>(metadata.readAll());
    metadata.close();

    if (m_track.empty())
        return;

    m_currentFrame = m_track.cbegin();

    connect(qnMetadataAnalyticsController, &MetadataAnalyticsController::frameReceived, this,
        [this](const QnResourcePtr& resource, qint64 timestampUs)
        {
            if (resource != m_resource)
                return;

            m_currentFrame = std::lower_bound(m_track.cbegin(), m_track.cend(), std::chrono::microseconds(timestampUs));
            if (m_currentFrame == m_track.cbegin() && timestampUs < m_currentFrame->timestampUsec)
            {
                qnMetadataAnalyticsController->gotMetadata(resource, {});
                return;
            }

            if (m_currentFrame != m_track.cbegin() && timestampUs < m_currentFrame->timestampUsec)
                --m_currentFrame;

            NX_EXPECT(m_currentFrame->timestampUsec <= timestampUs);

            nx::common::metadata::DetectionMetadataPacket metadata;
            metadata.objects = m_currentFrame->objects;
            qnMetadataAnalyticsController->gotMetadata(resource, std::move(metadata));
        });
}

bool LocalMetadataAnalyticsDriver::supportsAnalytics(const QnResourcePtr& resource)
{
    const auto file = resource.dynamicCast<QnAviResource>();
    if (!file)
        return false;

    QFileInfo metadata(metadataFileName(resource->getUrl()));
    return metadata.exists();
}

} // namespace desktop
} // namespace client
} // namespace nx
