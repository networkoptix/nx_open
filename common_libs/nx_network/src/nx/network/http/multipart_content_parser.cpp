#include "multipart_content_parser.h"

namespace nx_http {

MultipartContentParser::MultipartContentParser():
    m_state(waitingBoundary),
    m_nextState(none),
    m_contentLength((unsigned int)-1),   //-1 means no content-length is given
    m_chunkParseState(waitingEndOfLine)
{
}

MultipartContentParser::~MultipartContentParser()
{
}

bool MultipartContentParser::processData(const QnByteArrayConstRef& data)
{
    for (size_t offset = 0; offset < data.size(); )
    {
        switch (m_state)
        {
            case waitingBoundary:
            case readingHeaders:
            case readingTextData:
            {
                ConstBufferRefType lineBuffer;
                size_t bytesRead = 0;
                bool lineFound = m_lineSplitter.parseByLines(
                    data.mid(offset),
                    &lineBuffer,
                    &bytesRead);
                offset += bytesRead;
                if (!lineFound &&
                    m_lineSplitter.partialLineBuffer() == m_endBoundaryLine)
                {
                    m_lineSplitter.reset();
                    lineBuffer = m_endBoundaryLine;
                    lineFound = true;
                }
                if (!lineFound)
                    continue;

                if (!processLine(lineBuffer))
                    return false;
                break;
            }

            case depleteLineFeedBeforeBinaryData:
            {
                NX_ASSERT(offset < data.size());
                size_t bytesRead = 0;
                m_lineSplitter.finishCurrentLineEnding(data.mid(offset), &bytesRead);
                offset += bytesRead;
                m_state = m_nextState;
                break;
            }

            case readingSizedBinaryData:
            {
                NX_ASSERT(m_contentLength != (unsigned int)-1);
                const size_t bytesToRead =
                    std::min<size_t>(m_contentLength - m_currentFrame.size(), data.size() - offset);
                m_currentFrame += data.mid(offset, bytesToRead);
                offset += bytesToRead;
                if ((size_t)m_currentFrame.size() == m_contentLength)
                {
                    m_state = waitingBoundary;
                    if (!m_nextFilter->processData(m_currentFrame))
                        return false;
                    m_currentFrame.clear();
                    m_contentLength = (unsigned int)-1;
                }
                break;
            }

            case readingUnsizedBinaryData:
            {
                size_t localOffset = 0;
                readUnsizedBinaryData(data.mid(offset), &localOffset);
                offset += localOffset;
                break;
            }

            default:
                return false;
        }
    }

    if (m_state == eofReached)
        return m_nextFilter->processData(nx_http::BufferType()); //< Reporting eof with empty part.

    return true;
}

size_t MultipartContentParser::flush()
{
    switch (m_state)
    {
        case waitingBoundary:
        case readingHeaders:
        case readingTextData:
        {
            ConstBufferRefType lineBuffer = m_lineSplitter.flush();
            if (!lineBuffer.isEmpty())
            {
                processLine(lineBuffer);
                if (m_state == eofReached)
                    return m_nextFilter->processData(nx_http::BufferType()); //< Reporting eof with empty part.
            }
            break;
        }

        default:
            break;
    }

    if (m_currentFrame.isEmpty())
        return 0;

    m_currentFrame += std::move(m_supposedBoundary);
    m_supposedBoundary.clear();
    m_nextFilter->processData(m_currentFrame);
    const size_t frameSizeBak = m_currentFrame.size();
    m_currentFrame.clear();
    return frameSizeBak;
}

bool MultipartContentParser::setContentType(const StringType& contentType)
{
    static const char multipartContentType[] = "multipart/";

    // Analyzing response headers (if needed).
    const nx_http::StringType::value_type* sepPos =
        std::find(contentType.constData(), contentType.constData() + contentType.size(), ';');
    if (sepPos == contentType.constData() + contentType.size())
        return false;   //< Unexpected content type.

    if (nx_http::ConstBufferRefType(contentType, 0, sizeof(multipartContentType) - 1)
        != multipartContentType)
    {
        return false; //< Unexpected content type.
    }

    // Searching first not-space.
    const nx_http::StringType::value_type* boundaryStart = std::find_if(
        sepPos + 1,
        contentType.constData() + contentType.size(),
        std::not1(std::bind1st(std::equal_to<nx_http::StringType::value_type>(), ' ')));
    if (boundaryStart == contentType.constData() + contentType.size())
    {
        // Failed to read boundary marker.
        return false;
    }
    if (!nx_http::ConstBufferRefType(contentType, boundaryStart - contentType.constData())
            .startsWith("boundary="))
    {
        // Failed to read boundary marker.
        return false;
    }
    boundaryStart += sizeof("boundary=") - 1;
    setBoundary(contentType.mid(boundaryStart - contentType.constData()));

    return true;
}

void MultipartContentParser::setBoundary(const StringType& boundary)
{
    // Boundary can contain starting "--" depending on implementation.
    m_boundary = boundary.startsWith("--") ? boundary.mid(2, boundary.size() - 2) : boundary;
    // Dropping starting and trailing quotes.
    while (!m_boundary.isEmpty() && m_boundary[0] == '"')
        m_boundary.remove(0, 1);
    while (!m_boundary.isEmpty() && m_boundary[m_boundary.size() - 1] == '"')
        m_boundary.remove(m_boundary.size() - 1, 1);
    m_startBoundaryLine = "--" + m_boundary/*+"\r\n"*/; //--boundary\r\n
    m_startBoundaryForUnsizedBinaryParsing = "\r\n" + m_startBoundaryLine + "\r\n";
    m_startBoundaryForUnsizedBinaryParsingWOTrailingCRLF = "\r\n" + m_startBoundaryLine;
    m_endBoundaryLine = "--" + m_boundary + "--" /*"\r\n"*/;
    m_endBoundaryForUnsizedBinaryParsing =
        m_startBoundaryForUnsizedBinaryParsingWOTrailingCRLF + "--";
}

const nx_http::HttpHeaders& MultipartContentParser::prevFrameHeaders() const
{
    return m_currentFrameHeaders;
}

bool MultipartContentParser::processLine(const ConstBufferRefType& lineBuffer)
{
    switch (m_state)
    {
        case waitingBoundary:
            if (lineBuffer == m_startBoundaryLine || lineBuffer == m_endBoundaryLine)
            {
                m_state = lineBuffer == m_startBoundaryLine ? readingHeaders : eofReached;
                m_currentFrameHeaders.clear();
            }
            break;

        case readingTextData:
            if (lineBuffer == m_startBoundaryLine || lineBuffer == m_endBoundaryLine)
            {
                if (!m_nextFilter->processData(m_currentFrame))
                    return false;
                m_currentFrame.clear();

                m_state = lineBuffer == m_startBoundaryLine ? readingHeaders : eofReached;
                m_currentFrameHeaders.clear();
                break;
            }
            m_currentFrame += lineBuffer;
            break;

        case readingHeaders:
        {
            if (lineBuffer.isEmpty())
            {
                const auto contentLengthIter = m_currentFrameHeaders.find("Content-Length");
                if (contentLengthIter != m_currentFrameHeaders.end())
                {
                    // Content-Length is known.
                    m_contentLength = contentLengthIter->second.toUInt();
                    m_state = depleteLineFeedBeforeBinaryData;
                    m_nextState = readingSizedBinaryData;
                }
                else
                {
                    const nx_http::StringType& contentType =
                        nx_http::getHeaderValue(m_currentFrameHeaders, "Content-Type");
                    bool isTextData = !m_forceParseAsBinary
                        && (contentType == "application/text" || contentType == "text/plain");

                    if (isTextData)
                    {
                        m_state = readingTextData;
                    }
                    else
                    {
                        m_state = depleteLineFeedBeforeBinaryData;
                        m_nextState = readingUnsizedBinaryData;
                    }
                }
                break;
            }
            QnByteArrayConstRef headerName;
            QnByteArrayConstRef headerValue;
            nx_http::parseHeader(&headerName, &headerValue, lineBuffer); //< Ignoring result.
            m_currentFrameHeaders.emplace(headerName, headerValue);
            break;
        }

        default:
            NX_ASSERT(false);
            return false;
    }

    return true;
}

bool MultipartContentParser::readUnsizedBinaryData(
    QnByteArrayConstRef data,
    size_t* const offset)
{
    // TODO: #ak Move this function (splitting byte stream by a pattern) to a separate file.

    for (;;)
    {
        switch (m_chunkParseState)
        {
            case waitingEndOfLine:
            {
                const auto slashRPos = data.indexOf('\r');
                if (slashRPos == -1)
                {
                    m_currentFrame += data;
                    *offset = data.size();
                    return true;
                }
                // Saving data up to last found \r.
                m_currentFrame += data.mid(0, slashRPos);
                *offset = slashRPos;
                m_chunkParseState = checkingForBoundaryAfterEndOfLine;
                data.pop_front(slashRPos);
                m_supposedBoundary += '\r';
                data.pop_front();
                *offset += 1;
                // *offset points to \r.
                continue;
            }

            case checkingForBoundaryAfterEndOfLine:
            {
                if (!m_supposedBoundary.isEmpty())
                {
                    // If we are here, then m_supposedBoundary does not contain full boundary yet.

                    // Saving supposed boundary in local buffer.
                    const size_t bytesNeeded =
                        m_startBoundaryForUnsizedBinaryParsing.size() - m_supposedBoundary.size();
                    const int slashRPos = data.indexOf('\r');
                    if ((slashRPos != -1) &&
                        (static_cast<size_t>(slashRPos) < bytesNeeded) &&
                        !((m_supposedBoundary + data.mid(0, slashRPos))
                            .startsWith(m_startBoundaryForUnsizedBinaryParsingWOTrailingCRLF)))
                    {
                        // Boundary was not found, resetting boundary check.
                        m_currentFrame += m_supposedBoundary;
                        m_supposedBoundary.clear();
                        m_chunkParseState = waitingEndOfLine;
                        return true;
                    }

                    const size_t bytesToCopy = (data.size() > bytesNeeded)
                        ? bytesNeeded
                        : data.size();
                    m_supposedBoundary += data.mid(0, bytesToCopy);
                    *offset += bytesToCopy;
                    data.pop_front(bytesToCopy);
                }

                QnByteArrayConstRef supposedBoundary;
                if (m_supposedBoundary.size() == m_startBoundaryForUnsizedBinaryParsing.size()) //< Supposed boundary is in m_supposedBoundary buffer.
                {
                    supposedBoundary = QnByteArrayConstRef(m_supposedBoundary);
                }
                else if (m_supposedBoundary.isEmpty() &&
                    (data.size() >= (size_t)m_startBoundaryForUnsizedBinaryParsing.size())) //< Supposed boundary is in the source data.
                {
                    supposedBoundary = data.mid(0, m_startBoundaryForUnsizedBinaryParsing.size());
                    *offset += m_startBoundaryForUnsizedBinaryParsing.size();
                    data.pop_front(m_startBoundaryForUnsizedBinaryParsing.size());
                }
                else
                {
                    //waiting for more data
                    m_supposedBoundary += data;
                    *offset += data.size();
                    return true;
                }

                //checking if boundary has been met
                if ((supposedBoundary == m_startBoundaryForUnsizedBinaryParsing) ||
                    (supposedBoundary == m_endBoundaryForUnsizedBinaryParsing))
                {
                    //found frame delimiter
                    if (!m_nextFilter->processData(m_currentFrame))
                        return false;
                    m_currentFrame.clear();

                    m_state = supposedBoundary == m_endBoundaryForUnsizedBinaryParsing
                        ? eofReached
                        : readingHeaders;
                    m_currentFrameHeaders.clear();
                }
                else
                {
                    //not a boundary, just frame data...
                    m_currentFrame += supposedBoundary;
                }

                m_supposedBoundary.clear();
                m_chunkParseState = waitingEndOfLine;
                break;
            }
        }

        break;
    }

    return true;
}

bool MultipartContentParser::eof() const
{
    return m_state == eofReached;
}

void MultipartContentParser::setForceParseAsBinary(bool force)
{
    m_forceParseAsBinary = force;
}

} // namespace nx_http
