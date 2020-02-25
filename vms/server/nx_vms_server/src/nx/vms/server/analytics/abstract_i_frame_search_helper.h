#pragma once

#include <nx/utils/uuid.h>
#include <nx/vms/api/types_fwd.h>

namespace nx::vms::server::analytics {

/**
 * Allows to find the nearest I-frame next to the specified time. It is used in analytics, 
 * to round default bestShot timestamp to the nearest I-frame.
 */
class AbstractIFrameSearchHelper
{
public:
    virtual ~AbstractIFrameSearchHelper() {}

    /**
     * @param deviceId Device UUID.
     * @param objectTypeId Object type of an analytics track. It needs to match the corresponding
     * analytics engine and check its primary/secondary stream setting.
     * @param timestampUs Timestamp of a analytics packet.
     * @return Timestamp in microseconds if the I-frame with time >= timestampUs is found, or -1
     * otherwise.
     */
    virtual qint64 findAfter(
        const QnUuid& deviceId, 
        nx::vms::api::StreamIndex streamIndex,
        qint64 timestampUs) const = 0;
};

} // namespace nx::vms::server::analytics
