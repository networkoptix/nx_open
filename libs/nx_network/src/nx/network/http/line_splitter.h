// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/utils/buffer.h>

namespace nx::network::http {

/**
 * Splits source bytes stream to lines. Accepts any line ending: CR, LF, CRLF.
 */
class NX_NETWORK_API LineSplitter
{
public:
    virtual ~LineSplitter() = default;

    /**
     * If a line is found then true is returned and lineBuffer contains reference to the line.
     * If the whole line was found in the data then lineBuffer references that buffer.
     * Otherwise, lineBuffer references an internal buffer and is valid until the next
     * parseByLines call or until the object destruction.
     * @param bytesRead Always contains number of bytes read from the data.
     * The reading stops when a line is found. The line ending is consumed also.
     * NOTE: If data contains multiple lines, this method returns the first line and does not cache anything!
     */
    bool parseByLines(
        const ConstBufferRefType& data,
        ConstBufferRefType* lineBuffer,
        std::size_t* bytesRead);

    /**
     * Since this class allows CR, LF or CRLF line ending, it is possible that line,
     * actually terminated with CRLF will be found after CR only (if CR and LF come in different buffers).
     * This method will read that trailing LF.
     * NOTE: This method is needed if we do not want to call parseByLines anymore.
     *   E.g. we have read all HTTP headers and starting to read message body.
     *   Call this method to read trailing LF.
     */
    void finishCurrentLineEnding(
        const ConstBufferRefType& data,
        std::size_t* bytesRead);

    bool currentLineEndingClosed() const;

    /** Resets parser state. */
    void reset();

    ConstBufferRefType partialLineBuffer() const;

    /**
     * @return Contents of the internal cache.
     */
    ConstBufferRefType flush();

private:
    nx::Buffer m_currentLine;
    bool m_clearCurrentLineBuf = false;
    char m_prevLineEnding = 0;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API StringLineIterator
{
public:
    /**
     * NOTE: Does not copy str data. Stores std::string_view inside.
     */
    StringLineIterator(const std::string_view& str);

    std::optional<std::string_view> next();

private:
    const std::string_view m_sourceData;
    LineSplitter m_lineSplitter;
    std::size_t m_dataOffset = 0;
};

} // namespace nx::network::http
