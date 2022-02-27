// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>

#include "http_types.h"
#include "line_splitter.h"

namespace nx::network::http {

/**
 * Input: http multipart content stream.
 * Output: separate content frames.
 */
class NX_NETWORK_API MultipartContentParser:
    public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    virtual bool processData(const ConstBufferRefType& data) override;
    virtual size_t flush() override;

    /**
     * @param Expected to be similar to "multipart/x-mixed-replace;boundary=some_boundary".
     * @return False, if contentType does not specify multipart content.
     * NOTE: After this method has been called, no MultipartContentParser::setBoundary call is needed.
    */
    bool setContentType(const std::string& contentType);
    void setBoundary(const std::string& boundary);
    void setForceParseAsBinary(bool force);

    /**
     * @return Headers of last read frame.
     */
    const HttpHeaders& prevFrameHeaders() const;

    /**
     * @return True if epilogue has been received.
     */
    bool eof() const;

private:
    enum ParsingState
    {
        none,
        waitingBoundary,
        readingHeaders,
        readingTextData,
        /** Reading trailing CR or LF before binary data. */
        depleteLineFeedBeforeBinaryData,
        /** Reading data with Content-Length known. */
        readingSizedBinaryData,
        /** Reading data with Content-Length not known: searching for boundary. */
        readingUnsizedBinaryData,
        /** Epilogue has been received. */
        eofReached
    };

    enum ChunkParseState
    {
        waitingEndOfLine,
        checkingForBoundaryAfterEndOfLine
    };

    ParsingState m_state = waitingBoundary;
    ParsingState m_nextState = none;
    LineSplitter m_lineSplitter;
    nx::Buffer m_currentFrame;
    std::string m_boundary;
    std::string m_startBoundaryLine;
    std::string m_endBoundaryLine;
    std::string m_endBoundaryForUnsizedBinaryParsing;
    std::string m_startBoundaryForUnsizedBinaryParsing;
    std::string m_startBoundaryForUnsizedBinaryParsingWOTrailingCRLF;
    unsigned int m_contentLength = (unsigned int) -1;
    ChunkParseState m_chunkParseState = waitingEndOfLine;
    nx::Buffer m_supposedBoundary;
    nx::network::http::HttpHeaders m_currentFrameHeaders;
    bool m_forceParseAsBinary = false;

    bool processLine(const ConstBufferRefType& lineBuffer);

    bool readUnsizedBinaryData(
        ConstBufferRefType data,
        size_t* const offset);
};

} // namespace nx::network::http
