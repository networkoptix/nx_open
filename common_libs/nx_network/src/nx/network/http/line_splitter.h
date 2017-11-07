#pragma once

#include <boost/optional.hpp>

#include "http_types.h"

namespace nx_http {

/**
 * Splits source bytes stream to lines. Accepts any line ending: CR, LF, CRLF.
 */
class NX_NETWORK_API LineSplitter
{
public:
    LineSplitter();
    virtual ~LineSplitter() = default;

    /**
     * If source buffer data contains whole line then reference to data is returned, 
     *   otherwise - data is cached and returned reference to internal buffer.
     * *lineBuffer is used to return found line.
     * *lineBuffer can be pointer to data or to an internal buffer. 
     *   It is valid to next parseByLines call or to object destruction
     * @return true, if line read (it can be empty). false, if no line yet
     * NOTE: If data contains multiple lines, this method returns first line and does not cache anything!
     */
    bool parseByLines(
        const ConstBufferRefType& data,
        QnByteArrayConstRef* const lineBuffer,
        size_t* const bytesRead );
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
        size_t* const bytesRead );
    bool currentLineEndingClosed() const;
    /** Resets parser state. */
    void reset();
    QnByteArrayConstRef partialLineBuffer() const;
    QnByteArrayConstRef flush();

private:
    BufferType m_currentLine;
    bool m_clearCurrentLineBuf;
    char m_prevLineEnding;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API StringLineIterator
{
public:
    StringLineIterator(const nx::String& str);

    boost::optional<ConstBufferRefType> next();

private:
    const nx::String& m_sourceData;
    LineSplitter m_lineSplitter;
    std::size_t m_dataOffset = 0;
};

} // namespace nx_http
