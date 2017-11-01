#pragma once

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>

#include "http_types.h"

namespace nx_http {

class NX_NETWORK_API MultipartBodySerializer
{
public:
    /**
     * @param boundary Does not contain -- neither in the beginning nor in the end.
     */
    MultipartBodySerializer(
        StringType boundary,
        std::shared_ptr<nx::utils::bstream::AbstractByteStreamFilter> outputStream);
    virtual ~MultipartBodySerializer() = default;

    StringType contentType() const;

    /**
     * Ends current part (if any).
     * @param headers should not contain Content-Type header since it is inserted with contentType value.
     */
    void beginPart(
        const StringType& contentType,
        const nx_http::HttpHeaders& headers,
        BufferType data);
    /**
     * Can be called only after MultipartMessageBodySource::beginPart.
     */
    void writeData(BufferType data);

    /**
     * Writes whole part.
     * Includes Content-Length header if none present in headers.
     */
    void writeBodyPart(
        const StringType& contentType,
        const nx_http::HttpHeaders& headers,
        BufferType data);
    /** Signal end of multipart body. */
    void writeEpilogue();
    /**
     * @return true after MultipartBodySerializer::writeEpilogue has been called.
     */
    bool eof() const;

private:
    const StringType m_boundary;
    const StringType m_delimiter;
    std::shared_ptr<nx::utils::bstream::AbstractByteStreamFilter> m_outputStream;
    bool m_bodyPartStarted;
    bool m_epilogueWritten;

    void startBodyPartInternal(
        const StringType& contentType,
        const nx_http::HttpHeaders& headers,
        BufferType data,
        boost::optional<std::uint64_t> contentLength);
};

} // namespace nx_http
