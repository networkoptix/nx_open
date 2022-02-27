// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>

#include "http_types.h"

namespace nx::network::http {

class NX_NETWORK_API MultipartBodySerializer
{
public:
    /**
     * @param boundary Does not contain -- neither in the beginning nor in the end.
     */
    MultipartBodySerializer(
        std::string boundary,
        std::shared_ptr<nx::utils::bstream::AbstractByteStreamFilter> outputStream);
    virtual ~MultipartBodySerializer() = default;

    /**
     * @return Multipart content type to be used in HTTP response.
     * Each part can have its own content type.
     */
    std::string contentType() const;

    /**
     * Starts new part and ends the current one (if any).
     * @param headers Should not contain Content-Type header since it is inserted with contentType value.
     */
    void beginPart(
        const std::string& contentType,
        const nx::network::http::HttpHeaders& headers,
        nx::Buffer data);

    /**
     * Can be called only after MultipartMessageBodySource::beginPart.
     */
    void writeData(nx::Buffer data);

    /**
     * Writes whole part.
     * Includes Content-Length header if none present in headers.
     */
    void writeBodyPart(
        const std::string& contentType,
        const nx::network::http::HttpHeaders& headers,
        nx::Buffer data);

    /** Signal end of multipart body. */
    void writeEpilogue();

    /**
     * @return True after MultipartBodySerializer::writeEpilogue has been called.
     */
    bool eof() const;

private:
    const std::string m_boundary;
    const std::string m_delimiter;
    std::shared_ptr<nx::utils::bstream::AbstractByteStreamFilter> m_outputStream;
    bool m_bodyPartStarted;
    bool m_epilogueWritten;

    void startBodyPartInternal(
        const std::string& contentType,
        const nx::network::http::HttpHeaders& headers,
        nx::Buffer data,
        std::optional<std::uint64_t> contentLength);
};

} // namespace nx::network::http
