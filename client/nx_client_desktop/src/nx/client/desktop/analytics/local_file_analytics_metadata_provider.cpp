#include "local_file_analytics_metadata_provider.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/fusion/serialization/json_functions.h>
#include <core/resource/avi/avi_resource.h>

#include <ini.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

QString metadataFileName(QString fileName)
{
    return fileName.replace(QFileInfo(fileName).suffix(), lit("metadata"));
}

} // namespace

LocalFileAnalyticsMetadataProvider::LocalFileAnalyticsMetadataProvider(
    const QnResourcePtr& resource)
{
    QFile metadataFile(metadataFileName(resource->getUrl()));
    if (metadataFile.open(QIODevice::ReadOnly))
    {
        m_metadata =
            QJson::deserialized<std::vector<nx::common::metadata::DetectionMetadataPacket>>(
                metadataFile.readAll());
    }
}

common::metadata::DetectionMetadataPacketPtr LocalFileAnalyticsMetadataProvider::metadata(
    qint64 timestamp, int channel)
{
    if (m_metadata.empty())
        return common::metadata::DetectionMetadataPacketPtr();

    if (channel != 0)
        return common::metadata::DetectionMetadataPacketPtr();

    auto currentFrame = std::lower_bound(
        m_metadata.cbegin(), m_metadata.cend(), std::chrono::microseconds(timestamp));
    if (currentFrame == m_metadata.cend())
        --currentFrame;

    if (currentFrame == m_metadata.cbegin() && timestamp < currentFrame->timestampUsec)
        return common::metadata::DetectionMetadataPacketPtr();

    if (currentFrame != m_metadata.cbegin() && timestamp < currentFrame->timestampUsec)
        --currentFrame;

    NX_EXPECT(currentFrame->timestampUsec <= timestamp);

    return std::make_shared<common::metadata::DetectionMetadataPacket>(*currentFrame);
}

bool LocalFileAnalyticsMetadataProviderFactory::supportsAnalytics(
    const QnResourcePtr& resource) const
{
    if (!ini().externalMetadata)
        return false;

    if (resource.dynamicCast<QnAviResource>().isNull())
        return false;

    return QFile::exists(metadataFileName(resource->getUrl()));
}

core::AbstractAnalyticsMetadataProviderPtr
    LocalFileAnalyticsMetadataProviderFactory::createMetadataProvider(
        const QnResourcePtr& resource) const
{
    if (!supportsAnalytics(resource))
        return core::AbstractAnalyticsMetadataProviderPtr();

    return core::AbstractAnalyticsMetadataProviderPtr(
        new LocalFileAnalyticsMetadataProvider(resource));
}

} // namespace desktop
} // namespace client
} // namespace nx
