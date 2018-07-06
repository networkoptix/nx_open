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

bool metadataContainsTime(
    const nx::common::metadata::DetectionMetadataPacket& metadata,
    qint64 timestamp)
{
    return metadata.timestampUsec >= timestamp
        && timestamp < metadata.timestampUsec + metadata.durationUsec;
};

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
    qint64 timestamp, int channel) const
{
    const auto& metadataList = metadataRange(timestamp, timestamp + 1, channel);
    if (metadataList.isEmpty())
        return {};

    return metadataList.first();
}

QList<common::metadata::DetectionMetadataPacketPtr>
    LocalFileAnalyticsMetadataProvider::metadataRange(
        qint64 startTimestamp, qint64 endTimestamp, int channel, int maximumCount) const
{
    if (channel != 0)
        return {};

    auto startIt = std::lower_bound(
        std::next(m_metadata.cbegin()),
        m_metadata.cend(),
        std::chrono::microseconds(startTimestamp));

    if (startIt == m_metadata.cend() || startTimestamp < startIt->timestampUsec)
    {
        --startIt;
        if (!metadataContainsTime(*startIt, startTimestamp))
            ++startIt;
    }

    QList<common::metadata::DetectionMetadataPacketPtr> result;

    auto itemsLeft = maximumCount;
    auto it = startIt;
    while (itemsLeft != 0 && it != m_metadata.end() && it->timestampUsec < endTimestamp)
    {
        result.append(std::make_shared<common::metadata::DetectionMetadataPacket>(*it));

        ++it;
        --itemsLeft;
    }

    return result;
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
