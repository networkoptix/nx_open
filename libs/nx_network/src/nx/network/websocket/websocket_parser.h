// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/buffer.h>
#include <nx/utils/byte_stream/custom_output_stream.h>
#include <nx/utils/gzip/gzip_uncompressor.h>
#include <nx/utils/move_only_func.h>

#include "websocket_common_types.h"

namespace nx::network::websocket {

class NX_NETWORK_API Parser
{
    enum class ParseState
    {
        readingHeaderFixedPart,
        readingHeaderExtension,
        readingPayload,
    };

    enum class BufferedState
    {
        notNeeded,
        enough,
        needMore
    };

    const int kFixedHeaderLen = 2;

public:
    using GotFrameHandler = nx::utils::MoveOnlyFunc<void(FrameType, nx::Buffer&&, bool)>;

    Parser(Role role, GotFrameHandler gotFrameHandler);
    void consume(char* data, int len);
    void consume(nx::Buffer& buf);
    void setRole(Role role);
    FrameType frameType() const;
    int frameSize() const;

private:
    Role m_role;
    GotFrameHandler m_frameHandler;
    nx::Buffer m_buf;
    nx::Buffer m_frameBuffer;
    nx::Buffer m_uncompressed;
    ParseState m_state = ParseState::readingHeaderFixedPart;
    int m_pos = 0;
    int m_payloadLen = 0;
    int m_headerExtLen;
    FrameType m_opCode;
    bool m_fin = false;
    bool m_masked = false;
    unsigned int m_mask = 0;
    int m_maskPos;
    bool m_firstFrame = true;
    bool m_doUncompress = false;
    nx::utils::bstream::gzip::Uncompressor m_uncompressor;

    void parse(char* data, int len);
    void processPart(
        char* data, int len, int neededLen, ParseState (Parser::*processFunc)(char* data));
    ParseState processPayload(char* data, int len);
    void reset();
    ParseState readHeaderFixed(char* data);
    ParseState readHeaderExtension(char* data);
    BufferedState bufferDataIfNeeded(const char* data, int len, int neededLen);
    void handleFrame();
};

} // namespace nx::network::websocket
