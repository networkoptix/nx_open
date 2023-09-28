// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multipart_body_serializer.h"

namespace nx::network::http {

MultipartBodySerializer::MultipartBodySerializer(
    std::string boundary,
    std::shared_ptr<nx::utils::bstream::AbstractByteStreamFilter> outputStream)
:
    m_boundary(std::move(boundary)),
    m_delimiter("\r\n--" + m_boundary),
    m_outputStream(std::move(outputStream)),
    m_bodyPartStarted(false),
    m_epilogueWritten(false)
{
}

std::string MultipartBodySerializer::contentType() const
{
    return "multipart/x-mixed-replace;boundary=" + m_boundary;
}

void MultipartBodySerializer::beginPart(
    const std::string& contentType,
    const nx::network::http::HttpHeaders& headers,
    nx::Buffer data)
{
    startBodyPartInternal(
        contentType,
        headers,
        std::move(data),
        std::nullopt);
}

void MultipartBodySerializer::writeData(nx::Buffer data)
{
    NX_ASSERT(m_bodyPartStarted);
    m_outputStream->processData(std::move(data));
}

void MultipartBodySerializer::writeBodyPart(
    const std::string& contentType,
    const nx::network::http::HttpHeaders& headers,
    nx::Buffer data)
{
    auto contentLength = data.size();
    startBodyPartInternal(
        contentType,
        headers,
        std::move(data),
        contentLength);
    m_bodyPartStarted = false;
}

void MultipartBodySerializer::writeEpilogue()
{
    m_bodyPartStarted = false;
    m_epilogueWritten = true;
    m_outputStream->processData(m_delimiter+"--");
}

bool MultipartBodySerializer::eof() const
{
    return m_epilogueWritten;
}

void MultipartBodySerializer::startBodyPartInternal(
    const std::string& contentType,
    const nx::network::http::HttpHeaders& headers,
    nx::Buffer data,
    std::optional<std::uint64_t> contentLength)
{
    m_bodyPartStarted = true;

    nx::Buffer serializedData;
    nx::utils::buildString(&serializedData, m_delimiter, "\r\n", "Content-Type: ", contentType, "\r\n");

    serializeHeaders(headers, &serializedData);
    if (contentLength && (headers.find("Content-Length") == headers.end()))
    {
        nx::utils::buildString(
            &serializedData,
            "Content-Length: ", std::to_string(*contentLength), "\r\n");
    }
    nx::utils::buildString(&serializedData, "\r\n", (std::string_view) data);

    m_outputStream->processData(std::move(serializedData));
}

} // namespace nx::network::http
