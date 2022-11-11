// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/buffer.h>

#include "websocket_common_types.h"

namespace nx::network::websocket {

class NX_NETWORK_API Serializer
{
public:
    Serializer(bool masked, unsigned mask = 0);

    nx::Buffer prepareMessage(const nx::Buffer& payload, FrameType type, CompressionType compressionType);
    nx::Buffer prepareFrame(nx::Buffer payload, FrameType type, bool fin);

private:
    bool m_masked = false;
    bool m_doCompress = false;
    unsigned m_mask = 0;

    void setMasked(bool masked, unsigned mask = 0);
    int fillHeader(char* data, bool fin, FrameType opCode, int payloadLenType, int payloadLen);
};


} // nx::network::websocket
