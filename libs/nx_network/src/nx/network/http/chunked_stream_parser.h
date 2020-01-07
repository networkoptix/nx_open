#pragma once

#include <nx/network/buffer.h>
#include <nx/utils/qnbytearrayref.h>

#include "line_splitter.h"

namespace nx::network::http {

/**
 * Parses HTTP/1.1 chunked stream (rfc7230, 4.1).
 */
class ChunkedStreamParser
{
public:
    /**
     * Reports message body bytes to func.
     * @return Bytes processed count. The processing stops when EOF is detected.
     * NOTE: Does not cache the input stream.
     * TODO: #ak Make it non-template and move to cpp.
     */
    template<class Func>
    size_t parse(
        const QnByteArrayConstRef& data,
        Func func)
    {
        // TODO: #ak Split this function after it is merged to the cloud_backend branch.

        size_t currentOffset = 0;
        for (; currentOffset < data.size(); )
        {
            const size_t currentOffsetBak = currentOffset;

            const nx::Buffer::value_type currentChar = data[(unsigned int)currentOffset];
            switch (m_chunkStreamParseState)
            {
                case State::waitingChunkStart:
                    m_chunkStreamParseState = State::readingChunkSize;
                    m_currentChunkSize = 0;
                    m_currentChunkBytesRead = 0;
                    break;

                case State::readingChunkSize:
                    if ((currentChar >= '0' && currentChar <= '9') ||
                        (currentChar >= 'a' && currentChar <= 'f') ||
                        (currentChar >= 'A' && currentChar <= 'F'))
                    {
                        m_currentChunkSize <<= 4;
                        m_currentChunkSize += hexCharToInt(currentChar);
                    }
                    else if (currentChar == ';')
                    {
                        m_chunkStreamParseState = State::readingChunkExtension;
                    }
                    else if (currentChar == ' ')
                    {
                        // Skipping whitespace.
                    }
                    else if (currentChar == '\r' || currentChar == '\n')
                    {
                        // No extension?
                        m_chunkStreamParseState = State::skippingCRLF;
                        m_nextChunkStreamParseState = State::readingChunkData;
                        break;
                    }
                    else
                    {
                        // Parse error.
                        return size_t(-1);
                    }
                    ++currentOffset;
                    break;

                case State::readingChunkExtension:
                    // TODO: #ak Reading extension.
                    if (currentChar == '\r' || currentChar == '\n')
                    {
                        m_chunkStreamParseState = State::skippingCRLF;
                        m_nextChunkStreamParseState = State::readingChunkData;
                        break;
                    }
                    ++currentOffset;
                    break;

                case State::skippingCRLF:
                    // Supporting CR, LF and CRLF as line-ending, since some buggy rtsp-implementation can do that.
                    if ((m_lineEndingOffset < 2) &&
                        ((currentChar == '\r' && m_lineEndingOffset == 0) ||
                        (currentChar == '\n' && (m_lineEndingOffset == 0 || m_prevChar == '\r'))))
                    {
                        ++m_lineEndingOffset;
                        ++currentOffset;    //< Skipping.

                        if (!(m_prevChar == '\r' && currentChar == '\n'))
                            break;
                        // Setting proper state right after CRLF.
                    }

                    m_lineEndingOffset = 0;
                    m_chunkStreamParseState = m_nextChunkStreamParseState;
                    break;

                case State::readingChunkData:
                {
                    if (!m_currentChunkSize)
                    {
                        // The last chunk.
                        m_chunkStreamParseState = State::readingTrailer;
                        break;
                    }

                    const size_t bytesToCopy = std::min<>(
                        m_currentChunkSize - m_currentChunkBytesRead,
                        data.size() - currentOffset);
                    func(data.mid(currentOffset, bytesToCopy));
                    m_currentChunkBytesRead += bytesToCopy;
                    currentOffset += bytesToCopy;
                    if (m_currentChunkBytesRead == m_currentChunkSize)
                    {
                        m_chunkStreamParseState = State::skippingCRLF;
                        m_nextChunkStreamParseState = State::waitingChunkStart;
                    }
                    break;
                }

                case State::readingTrailer:
                {
                    ConstBufferRefType lineBuffer;
                    size_t bytesRead = 0;
                    const bool lineFound = m_lineSplitter.parseByLines(
                        data.mid(currentOffset),
                        &lineBuffer,
                        &bytesRead);
                    currentOffset += bytesRead;
                    if (!lineFound)
                        break;
                    if (lineBuffer.isEmpty())
                    {
                        // Reached end of HTTP/1.1 chunk stream.
                        if (m_lineSplitter.currentLineEndingClosed())
                        {
                            m_chunkStreamParseState = State::reachedChunkStreamEnd;
                            return currentOffset;
                        }
                        else
                        {
                            m_lineEndingOffset = 1;
                            m_chunkStreamParseState = State::skippingCRLF;
                            m_nextChunkStreamParseState = State::reachedChunkStreamEnd;
                            break;
                        }
                    }
                    // TODO #ak Parsing entity-header.
                    break;
                }

                case State::reachedChunkStreamEnd:
                    NX_ASSERT(currentOffset > 0);
                    return currentOffset;

                default:
                    NX_ASSERT(false);
                    break;
            }

            if (currentOffset != currentOffsetBak) //< Moved cursor.
                m_prevChar = currentChar;
        }

        return currentOffset;
    }

    bool eof() const
    {
        return m_chunkStreamParseState == State::reachedChunkStreamEnd;
    }

    void reset()
    {
        m_chunkStreamParseState = State::waitingChunkStart;
        m_currentChunkSize = 0;
        m_currentChunkBytesRead = 0;
        m_prevChar = 0;
    }

private:
    enum class State
    {
        waitingChunkStart,
        readingChunkSize,
        readingChunkExtension,
        skippingCRLF,
        readingChunkData,
        readingTrailer,
        reachedChunkStreamEnd,
        undefined,
    };

    State m_chunkStreamParseState = State::waitingChunkStart;
    State m_nextChunkStreamParseState = State::undefined;
    size_t m_currentChunkSize = 0;
    size_t m_currentChunkBytesRead = 0;
    nx::Buffer::value_type m_prevChar = 0;
    int m_lineEndingOffset = 0;
    LineSplitter m_lineSplitter;

private:
    unsigned int hexCharToInt(nx::Buffer::value_type ch)
    {
        if (ch >= '0' && ch <= '9')
            return ch - '0';
        if (ch >= 'a' && ch <= 'f')
            return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F')
            return ch - 'A' + 10;
        return 0;
    }
};

} // namespace nx::network::http
