// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multipart_content_parser.h"

namespace nx::network::http {

bool MultipartContentParser::processData(const ConstBufferRefType& data)
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
                    data.substr(offset),
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
                m_lineSplitter.finishCurrentLineEnding(data.substr(offset), &bytesRead);
                offset += bytesRead;
                m_state = m_nextState;
                break;
            }

            case readingSizedBinaryData:
            {
                NX_ASSERT(m_contentLength != (unsigned int)-1);
                const size_t bytesToRead =
                    std::min<size_t>(m_contentLength - m_currentFrame.size(), data.size() - offset);
                m_currentFrame += data.substr(offset, bytesToRead);
                offset += bytesToRead;
                if ((size_t)m_currentFrame.size() == m_contentLength)
                {
                    m_state = waitingBoundary;

                    // TODO #szaitsev: no 'if' and no 'return' should be here.
                    // This fix should be done and tested in 4.1.
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
                readUnsizedBinaryData(data.substr(offset), &localOffset);
                offset += localOffset;
                break;
            }

            default:
                return false;
        }
    }

    if (m_state == eofReached)
        return m_nextFilter->processData(ConstBufferRefType()); //< Reporting eof with empty part.

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
            if (!lineBuffer.empty())
            {
                processLine(lineBuffer);
                if (m_state == eofReached)
                    return m_nextFilter->processData(ConstBufferRefType()); //< Reporting eof with empty part.
            }
            break;
        }

        default:
            break;
    }

    if (m_currentFrame.empty())
        return 0;

    m_currentFrame += std::move(m_supposedBoundary);
    m_supposedBoundary.clear();
    m_nextFilter->processData(m_currentFrame);
    const size_t frameSizeBak = m_currentFrame.size();
    m_currentFrame.clear();
    return frameSizeBak;
}

bool MultipartContentParser::setContentType(const std::string& str)
{
    const auto [tokens, tokenCount] = nx::utils::split_n<2>(str, ';');
    if (tokenCount != 2)
        return false; //< Unexpected content type.

    const auto& contentType = tokens[0];
    if (!nx::utils::startsWith(contentType, "multipart/"))
        return false;

    // "boundary=xxx"
    const auto& boundaryStr = nx::utils::trim(tokens[1]);
    const auto [boundaryTokens, boundaryTokenCount] = nx::utils::split_n<2>(boundaryStr, '=');
    if (boundaryTokens[0] != "boundary" || boundaryTokens[1].empty())
        return false;

    setBoundary(std::string(boundaryTokens[1]));
    return true;
}

void MultipartContentParser::setBoundary(const std::string& boundary)
{
    // Boundary can contain starting "--" depending on implementation.
    m_boundary = nx::utils::startsWith(boundary, "--")
        ? boundary.substr(2, boundary.size() - 2)
        : boundary;
    
    // Dropping starting and trailing quotes.
    while (!m_boundary.empty() && m_boundary[0] == '"')
        m_boundary.erase(0, 1);
    while (!m_boundary.empty() && m_boundary[m_boundary.size() - 1] == '"')
        m_boundary.erase(m_boundary.size() - 1, 1);
    
    m_startBoundaryLine = "--" + m_boundary/*+"\r\n"*/; //--boundary\r\n
    m_startBoundaryForUnsizedBinaryParsing = "\r\n" + m_startBoundaryLine + "\r\n";
    m_startBoundaryForUnsizedBinaryParsingWOTrailingCRLF = "\r\n" + m_startBoundaryLine;
    m_endBoundaryLine = "--" + m_boundary + "--" /*"\r\n"*/;
    m_endBoundaryForUnsizedBinaryParsing =
        m_startBoundaryForUnsizedBinaryParsingWOTrailingCRLF + "--";
}

const nx::network::http::HttpHeaders& MultipartContentParser::prevFrameHeaders() const
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
            if (lineBuffer.empty())
            {
                const auto contentLengthIter = m_currentFrameHeaders.find("Content-Length");
                if (contentLengthIter != m_currentFrameHeaders.end())
                {
                    // Content-Length is known.
                    m_contentLength = nx::utils::stoul(contentLengthIter->second);
                    m_state = depleteLineFeedBeforeBinaryData;
                    m_nextState = readingSizedBinaryData;
                }
                else
                {
                    const std::string& contentType =
                        nx::network::http::getHeaderValue(m_currentFrameHeaders, "Content-Type");
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
            
            std::string headerName;
            std::string headerValue;
            parseHeader(lineBuffer, &headerName, &headerValue);
            m_currentFrameHeaders.emplace(std::move(headerName), std::move(headerValue));
            break;
        }

        default:
            NX_ASSERT(false);
            return false;
    }

    return true;
}

bool MultipartContentParser::readUnsizedBinaryData(
    ConstBufferRefType data,
    size_t* const offset)
{
    // TODO: #akolesnikov Move this function (splitting byte stream by a pattern) to a separate file.

    for (;;)
    {
        switch (m_chunkParseState)
        {
            case waitingEndOfLine:
            {
                const auto slashRPos = data.find('\r');
                if (slashRPos == data.npos)
                {
                    m_currentFrame += data;
                    *offset = data.size();
                    return true;
                }

                // Saving data up to last found \r.
                m_currentFrame += data.substr(0, slashRPos);
                *offset = slashRPos;
                m_chunkParseState = checkingForBoundaryAfterEndOfLine;
                data.remove_prefix(slashRPos);
                m_supposedBoundary += '\r';
                data.remove_prefix(1);
                *offset += 1;
                // *offset points to \r.
                continue;
            }

            case checkingForBoundaryAfterEndOfLine:
            {
                if (!m_supposedBoundary.empty())
                {
                    // If we are here, then m_supposedBoundary does not contain full boundary yet.

                    // Saving supposed boundary in local buffer.
                    const size_t bytesNeeded =
                        m_startBoundaryForUnsizedBinaryParsing.size() - m_supposedBoundary.size();
                    const auto slashRPos = data.find('\r');
                    if ((slashRPos != data.npos) && (slashRPos < bytesNeeded) &&
                        !nx::utils::startsWith(
                            m_supposedBoundary + data.substr(0, slashRPos),
                            m_startBoundaryForUnsizedBinaryParsingWOTrailingCRLF))
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
                    m_supposedBoundary += data.substr(0, bytesToCopy);
                    *offset += bytesToCopy;
                    data.remove_prefix(bytesToCopy);
                }

                ConstBufferRefType supposedBoundary;
                if (m_supposedBoundary.size() == m_startBoundaryForUnsizedBinaryParsing.size()) //< Supposed boundary is in m_supposedBoundary buffer.
                {
                    supposedBoundary = ConstBufferRefType(m_supposedBoundary);
                }
                else if (m_supposedBoundary.empty() &&
                    (data.size() >= (size_t)m_startBoundaryForUnsizedBinaryParsing.size())) //< Supposed boundary is in the source data.
                {
                    supposedBoundary = data.substr(0, m_startBoundaryForUnsizedBinaryParsing.size());
                    *offset += m_startBoundaryForUnsizedBinaryParsing.size();
                    data.remove_prefix(m_startBoundaryForUnsizedBinaryParsing.size());
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

} // namespace nx::network::http
