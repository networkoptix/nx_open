// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/buffer.h>

#include "websocket_common_types.h"

namespace nx::network::websocket {

class NX_NETWORK_API Serializer
{
public:
    Serializer(bool masked, unsigned mask = 0);
    ~Serializer();

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Compression, gzip, deflate)

    static std::optional<Compression> defaultCompression(
        CompressionType compressionType, FrameType frameType)
    {
        return
            compressionType == CompressionType::none
                ? std::nullopt
                : frameType == FrameType::text
                    ? std::optional<Compression>{Compression::deflate}
                    : std::optional<Compression>{Compression::gzip};
    }

    nx::Buffer prepareMessage(
        const nx::Buffer& payload, FrameType type, std::optional<Compression> compression = {});
    nx::Buffer prepareFrame(
        nx::Buffer payload, FrameType type, bool fin, std::optional<Compression> compression);

    Statistics statistics() const { return m_statistics; }

private:
    struct Private;
    std::unique_ptr<Private> d;
    bool m_masked = false;
    unsigned m_mask = 0;
    Statistics m_statistics;

    void setMasked(bool masked, unsigned mask = 0);
    int fillHeader(
        char* data,
        bool fin,
        FrameType opCode,
        int payloadLenType,
        int payloadLen,
        bool compression);
};


} // nx::network::websocket
