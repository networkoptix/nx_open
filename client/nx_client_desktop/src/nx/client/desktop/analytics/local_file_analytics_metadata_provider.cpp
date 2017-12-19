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

enum class SearchPolicy
{
    exact,
    closestBefore,
    closestAfter
};

std::vector<nx::common::metadata::DetectionMetadataPacket>::const_iterator
    findMetadataIterator(
        const std::vector<nx::common::metadata::DetectionMetadataPacket>& metadata,
        qint64 timestamp,
        SearchPolicy searchPolicy)
{
    if (metadata.empty())
        return metadata.end();

    switch (searchPolicy)
    {
        case SearchPolicy::exact:
        {
            auto containsTime =
                [](const nx::common::metadata::DetectionMetadataPacket& metadata, qint64 timestamp)
                {
                    return metadata.timestampUsec >= timestamp
                        && timestamp < metadata.timestampUsec + metadata.durationUsec;
                };

            auto it = std::lower_bound(
                std::next(metadata.begin()),
                metadata.end(),
                std::chrono::microseconds(timestamp));

            if (it != metadata.end() && containsTime(*it, timestamp))
                return it;

            --it;

            if (containsTime(*it, timestamp))
                return it;

            return metadata.end();
        }

        case SearchPolicy::closestAfter:
            return std::lower_bound(metadata.begin(), metadata.end(),
                std::chrono::microseconds(timestamp));

        case SearchPolicy::closestBefore:
            return std::upper_bound(metadata.begin(), metadata.end(),
                std::chrono::microseconds(timestamp));
    }

    return metadata.end();
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
    qint64 timestamp, int channel) const
{
    if (channel != 0)
        return {};

    const auto it = findMetadataIterator(m_metadata, timestamp, SearchPolicy::exact);
    if (it == m_metadata.cend())
        return {};

    return std::make_shared<common::metadata::DetectionMetadataPacket>(*it);
}

QList<common::metadata::DetectionMetadataPacketPtr>
    LocalFileAnalyticsMetadataProvider::metadataRange(
        qint64 startTimestamp, qint64 endTimestamp, int channel, int maximumCount) const
{
    if (channel != 0)
        return {};

    const auto startIt = findMetadataIterator(
        m_metadata, startTimestamp, SearchPolicy::closestAfter);
    if (startIt == m_metadata.cend())
        return {};

    const auto endIt = findMetadataIterator(
        m_metadata, endTimestamp, SearchPolicy::closestBefore);
    if (startIt == m_metadata.cend())
        return {};

    QList<common::metadata::DetectionMetadataPacketPtr> result;
    auto itemsLeft = maximumCount;
    for (auto it = startIt; itemsLeft != 0 && it != endIt; ++it, --itemsLeft)
        result.append(std::make_shared<common::metadata::DetectionMetadataPacket>(*it));

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
