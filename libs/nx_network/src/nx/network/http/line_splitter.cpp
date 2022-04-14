// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "line_splitter.h"

#include <algorithm>

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
        m_currentLine.append(data.data(), (int)data.size());
        if (bytesRead)
            *bytesRead += data.size();
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

StringLineIterator::StringLineIterator(const std::string_view& str):
    m_sourceData(str)
{
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
