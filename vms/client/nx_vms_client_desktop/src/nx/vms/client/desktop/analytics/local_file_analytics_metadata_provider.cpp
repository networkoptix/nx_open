#include "local_file_analytics_metadata_provider.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/fusion/serialization/json_functions.h>
#include <core/resource/avi/avi_resource.h>

#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;
using namespace nx::common::metadata;

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
            QJson::deserialized<std::vector<ObjectMetadataPacket>>(metadataFile.readAll());
    }
}

ObjectMetadataPacketPtr LocalFileAnalyticsMetadataProvider::metadata(
    microseconds timestamp, int channel) const
{
    const auto& metadataList = metadataRange(timestamp, timestamp + 1us, channel, 1);
    if (metadataList.isEmpty())
        return {};

    return metadataList.first();
}

QList<ObjectMetadataPacketPtr> LocalFileAnalyticsMetadataProvider::metadataRange(
    microseconds startTimestamp,
    microseconds endTimestamp,
    int channel,
    int maximumCount) const
{
    if (channel != 0)
        return {};

    QList<ObjectMetadataPacketPtr> result;

    auto it = std::lower_bound(m_metadata.cbegin(), m_metadata.cend(), startTimestamp);

    if (it->timestampUs > startTimestamp.count() && it != m_metadata.cbegin())
    {
        --it;
        if (it->durationUs > 0 && it->timestampUs + it->durationUs < startTimestamp.count())
            ++it;
    }

    while (maximumCount > 0 && it != m_metadata.end() && it->timestampUs < endTimestamp.count())
    {
        result.append(std::make_shared<ObjectMetadataPacket>(*it));

        ++it;
        --maximumCount;
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

} // namespace nx::vms::client::desktop
