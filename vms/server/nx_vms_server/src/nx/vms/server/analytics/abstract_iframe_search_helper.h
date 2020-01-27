#pragma once

#include <nx/utils/uuid.h>
#include <nx/vms/api/types_fwd.h>

namespace nx::vms::server::analytics {

/**
 * This helper class allow to find nearest I-frame next to specified time. It is used in analytics, 
 * to round default bestShot timestamp to the nearest I-frame.
 */
class AbstractIFrameSearchHelper
{
public:
    virtual ~AbstractIFrameSearchHelper() {}

    /**
     * @param deviceId device UUID
     * @param objectTypeId objectType of a analytics track. It need to match corresponding
     * analytics engine and check it primary/secondary stream setting.
     * @param timestampUs timestamp of a analytics packet.
     * @return timestamp in microseconds if I-frame with time >= timeStampUs is found or -1.
     */
    virtual qint64 findAfter(
        const QnUuid& deviceId, 
        nx::vms::api::StreamIndex streamIndex,
        qint64 timestampUs) const = 0;
};

} // namespace nx::vms::server::analytics
