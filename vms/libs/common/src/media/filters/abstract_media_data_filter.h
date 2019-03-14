#pragma once

#include <nx/streaming/abstract_data_packet.h>

#ifdef ENABLE_DATA_PROVIDERS

class AbstractMediaDataFilter
{
public:
    virtual ~AbstractMediaDataFilter() = default;

    /**
     * Whether to copy source data or perform in-place processing is up to implementation.
     * @param data Source data.
     * @return Modified data.
    */
    virtual QnAbstractDataPacketPtr processData(const QnAbstractDataPacketPtr& data) = 0;
};
using AbstractMediaDataFilterPtr = std::shared_ptr<AbstractMediaDataFilter>;

#endif // ENABLE_DATA_PROVIDERS
