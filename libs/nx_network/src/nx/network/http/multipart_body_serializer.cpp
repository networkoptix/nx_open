#include "multipart_body_serializer.h"

namespace nx {
namespace network {
namespace http {

MultipartBodySerializer::MultipartBodySerializer(
    StringType boundary,
    std::shared_ptr<nx::utils::bstream::AbstractByteStreamFilter> outputStream)
:
    m_boundary(std::move(boundary)),
    m_delimiter("\r\n--"+m_boundary),
    m_outputStream(std::move(outputStream)),
    m_bodyPartStarted(false),
    m_epilogueWritten(false)
{
}

StringType MultipartBodySerializer::contentType() const
{
    return "multipart/x-mixed-replace;boundary="+m_boundary;
}

void MultipartBodySerializer::beginPart(
    const StringType& contentType,
    const nx::network::http::HttpHeaders& headers,
    BufferType data)
{
    startBodyPartInternal(
        contentType,
        headers,
        std::move(data),
        boost::none);
}

void MultipartBodySerializer::writeData(BufferType data)
{
    NX_ASSERT(m_bodyPartStarted);
    m_outputStream->processData(std::move(data));
}

void MultipartBodySerializer::writeBodyPart(
    const StringType& contentType,
    const nx::network::http::HttpHeaders& headers,
    BufferType data)
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
    const StringType& contentType,
    const nx::network::http::HttpHeaders& headers,
    BufferType data,
    boost::optional<std::uint64_t> contentLength)
{
    m_bodyPartStarted = true;

    BufferType serializedData;
    // TODO: #ak It makes sense to make a rough estimation of required serializedData size and reserve.

    serializedData +=
        m_delimiter +
        "\r\n"
        "Content-Type: " + contentType + "\r\n";
    serializeHeaders(headers, &serializedData);
    if (contentLength && (headers.find("Content-Length") == headers.end()))
        serializedData+= "Content-Length: "+BufferType::number((quint64)*contentLength)+"\r\n";
    serializedData += "\r\n";
    serializedData += data;

    m_outputStream->processData(std::move(serializedData));
}

} // namespace nx
} // namespace network
} // namespace http
