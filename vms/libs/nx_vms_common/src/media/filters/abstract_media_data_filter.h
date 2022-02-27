// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/streaming/abstract_data_packet.h>

class AbstractMediaDataFilter
{
public:
    virtual ~AbstractMediaDataFilter() = default;

    /**
     * Whether to copy source data or perform in-place processing is up to implementation.
     * @param data Source data.
     * @return Modified data.
    */
    virtual QnConstAbstractDataPacketPtr processData(const QnConstAbstractDataPacketPtr& data) = 0;
};
using AbstractMediaDataFilterPtr = std::shared_ptr<AbstractMediaDataFilter>;
