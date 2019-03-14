#include "line_splitter.h"

#include <algorithm>

namespace nx {
namespace network {
namespace http {

LineSplitter::LineSplitter():
    m_clearCurrentLineBuf(false),
    m_prevLineEnding(0)
{
}

bool LineSplitter::parseByLines(
    const ConstBufferRefType& data,
    QnByteArrayConstRef* const lineBuffer,
    size_t* const bytesRead)
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
    if (m_currentLine.isEmpty())
    {
        // Current line not cached, we're able to return reference to input data.
        *lineBuffer = data.mid(0, lineEnd - data.data());
    }
    else
    {
        m_currentLine.append(data.data(), lineEnd - data.data());
        *lineBuffer = QnByteArrayConstRef(m_currentLine);
        m_clearCurrentLineBuf = true;
    }
    m_prevLineEnding = *lineEnd;

    // TODO: #ak Skip \n in case when it comes with next buffer.
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
    if (data.isEmpty())
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

QnByteArrayConstRef LineSplitter::partialLineBuffer() const
{
    return m_currentLine;
}

QnByteArrayConstRef LineSplitter::flush()
{
    return m_currentLine;
}

//-------------------------------------------------------------------------------------------------

StringLineIterator::StringLineIterator(const nx::String& str):
    m_sourceData(str)
{
}

boost::optional<ConstBufferRefType> StringLineIterator::next()
{
    QnByteArrayConstRef line;
    size_t bytesRead = 0;
    if (m_lineSplitter.parseByLines(
            nx::network::http::ConstBufferRefType(m_sourceData, m_dataOffset), &line, &bytesRead))
    {
        m_dataOffset += bytesRead;
        return line;
    }

    return boost::none;
}

} // namespace nx
} // namespace network
} // namespace http
