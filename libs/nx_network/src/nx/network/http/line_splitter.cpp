// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "line_splitter.h"

#include <algorithm>
#include <limits>

namespace nx::network::http {

bool LineSplitter::parseByLines(
    const ConstBufferRefType& data,
    ConstBufferRefType* lineBuffer,
    size_t* bytesRead)
{
    if (bytesRead)
        *bytesRead = 0;

    if (m_clearCurrentLineBuf)
    {
        m_currentLine.clear();
        m_clearCurrentLineBuf = false;
    }

    // Searching line end in data.
    static const char CRLF[] = "\r\n";
    const char* lineEnd = std::find_first_of(
        data.data(),
        data.data() + data.size(),
        CRLF,
        CRLF + sizeof(CRLF) - 1);
    if (lineEnd == data.data() + data.size())
    {
        // Not found, caching input data.
        if (bytesRead)
            *bytesRead += data.size();
        if (m_currentLine.size() + data.size() > m_maxLineLength)
        {
            // Refusing to keep buffering an unterminated line without bound (ANAS-323).
            m_lineLengthExceeded = true;
            return false;
        }
        m_currentLine.append(data.data(), (int) data.size());
        return false;
    }

    if ((m_prevLineEnding == '\r') && (*lineEnd == '\n') && (lineEnd == data.data()))    //< Found LF just after CR.
    {
        // Reading trailing line ending.
        m_prevLineEnding = *lineEnd;
        if (bytesRead)
            *bytesRead += 1;
        return false;
    }

    if (m_currentLine.size() + (lineEnd - data.data()) > m_maxLineLength)
    {
        // Line terminator found within this call, but combined with any previously buffered
        // fragment the total line length still exceeds the cap (ANAS-323).
        m_lineLengthExceeded = true;
        if (bytesRead)
            *bytesRead += data.size();
        return false;
    }

    // Line feed found.
    if (m_currentLine.empty())
    {
        // Current line not cached, we're able to return reference to input data.
        *lineBuffer = data.substr(0, lineEnd - data.data());
    }
    else
    {
        m_currentLine.append(data.data(), lineEnd - data.data());
        *lineBuffer = ConstBufferRefType(m_currentLine);
        m_clearCurrentLineBuf = true;
    }
    m_prevLineEnding = *lineEnd;

    // TODO: #akolesnikov Skip \n in case when it comes with next buffer.
    if (*lineEnd == '\r' && (lineEnd + 1) < data.data() + data.size() && *(lineEnd + 1) == '\n')
    {
        m_prevLineEnding = '\n';
        ++lineEnd;
    }

    if (bytesRead)
        *bytesRead += lineEnd - data.data() + 1;
    return true;
}

void LineSplitter::finishCurrentLineEnding(
    const ConstBufferRefType& data,
    size_t* const bytesRead)
{
    if (bytesRead)
        *bytesRead = 0;
    if (data.empty())
        return;

    if ((m_prevLineEnding == '\r') && (data[0] == '\n'))    //< Found LF just after CR.
    {
        // Reading trailing line ending.
        m_prevLineEnding = data[0];
        if (bytesRead)
            *bytesRead += 1;
        return;
    }
}

bool LineSplitter::currentLineEndingClosed() const
{
    return m_prevLineEnding == '\n';
}

void LineSplitter::reset()
{
    m_currentLine.clear();
    m_clearCurrentLineBuf = false;
    m_prevLineEnding = 0;
    m_lineLengthExceeded = false;
}

void LineSplitter::setMaxLineLength(std::size_t maxLineLength)
{
    m_maxLineLength = maxLineLength;
}

std::size_t LineSplitter::maxLineLength() const
{
    return m_maxLineLength;
}

bool LineSplitter::lineLengthExceeded() const
{
    return m_lineLengthExceeded;
}

ConstBufferRefType LineSplitter::partialLineBuffer() const
{
    return m_currentLine;
}

ConstBufferRefType LineSplitter::flush()
{
    return m_currentLine;
}

//-------------------------------------------------------------------------------------------------

StringLineIterator::StringLineIterator(std::string_view str):
    m_sourceData(str)
{
    // This iterator splits an already fully in-memory buffer, not an attacker-controlled
    // incoming stream, so the streaming DoS LineSplitter::kDefaultMaxLineLength guards against
    // (ANAS-323) does not apply here. Leaving the default cap in place would silently truncate
    // legitimate long lines (e.g. m3u::Playlist::parse), so disable it.
    m_lineSplitter.setMaxLineLength(std::numeric_limits<std::size_t>::max());
}

std::optional<std::string_view> StringLineIterator::next()
{
    if (m_dataOffset >= m_sourceData.size())
        return std::nullopt;

    ConstBufferRefType line;
    size_t bytesRead = 0;
    if (m_lineSplitter.parseByLines(m_sourceData.substr(m_dataOffset), &line, &bytesRead))
    {
        m_dataOffset += bytesRead;
        return (std::string_view) line;
    }

    // Reporting text remainder as a line.
    line = m_sourceData.substr(m_dataOffset);
    m_dataOffset = m_sourceData.size();
    return (std::string_view) line;
}

} // namespace nx::network::http
