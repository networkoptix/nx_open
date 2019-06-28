#include "analytics_events_receptor.h"

#include <nx/fusion/model_functions.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/utils/log/log.h>

#include <analytics/common/object_detection_metadata.h>
#include <analytics/db/abstract_storage.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

namespace nx::analytics::db {

AnalyticsEventsReceptor::AnalyticsEventsReceptor(
    QnCommonModule* commonModule,
    AbstractEventsStorage* eventsStorage)
    :
    QnCommonModuleAware(commonModule),
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
        NX_WARNING(this, lm("Failed to parse detection metadata packet"));
        return;
    }

    auto resourcePool = commonModule()->resourcePool();
    if (auto server =
        resourcePool->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID()))
    {
        auto storage = resourcePool->getResourceById(server->metadataStorageId());
        if (storage && storage->getStatus() != Qn::Online)
        {
            NX_DEBUG(this, "Skip writing metadata record to the analytics database because metadata storage is offline");
            return;
        }
    }

    m_eventsStorage->save(std::move(detectionMetadataPacket));
}

} // namespace nx::analytics::db
