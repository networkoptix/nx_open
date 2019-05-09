#include "analytics_events_receptor.h"

#include <nx/fusion/model_functions.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/utils/log/log.h>

#include <analytics/common/object_detection_metadata.h>
#include <analytics/db/abstract_storage.h>

namespace nx::analytics::db {

AnalyticsEventsReceptor::AnalyticsEventsReceptor(
    AbstractEventsStorage* eventsStorage)
    :
    m_eventsStorage(eventsStorage)
{
}

bool AnalyticsEventsReceptor::canAcceptData() const
{
    // TODO: #ak Delegate to m_eventsStorage.
    return true;
}

void AnalyticsEventsReceptor::putData(const QnAbstractDataPacketPtr& data)
{
    QnCompressedMetadataPtr metadataPacket =
        std::dynamic_pointer_cast<QnCompressedMetadata>(data);
    if (!metadataPacket)
        return;

    auto detectionMetadataPacket =
        std::make_shared<nx::common::metadata::DetectionMetadataPacket>();

    bool isParsedSuccessfully = false;
    *detectionMetadataPacket =
        QnUbjson::deserialized<nx::common::metadata::DetectionMetadataPacket>(
            QByteArray::fromRawData(metadataPacket->data(),
            (int) metadataPacket->dataSize()),
            nx::common::metadata::DetectionMetadataPacket(),
            &isParsedSuccessfully);
    if (!isParsedSuccessfully)
    {
        NX_INFO(this, lm("Failed to parse detection metadata packet"));
        return;
    }

    m_eventsStorage->save(std::move(detectionMetadataPacket));
}

} // namespace nx::analytics::db
