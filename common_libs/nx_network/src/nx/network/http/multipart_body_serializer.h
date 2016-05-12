/**********************************************************
* May 12, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <utils/media/abstract_byte_stream_filter.h>

#include "httptypes.h"


namespace nx_http {

class NX_NETWORK_API MultipartBodySerializer
{
public:
    /**
        @param boundary Does not contain -- neither in the beginning nor in the end
    */
    MultipartBodySerializer(
        StringType boundary,
        std::shared_ptr<AbstractByteStreamFilter> outputStream);
    virtual ~MultipartBodySerializer();

    StringType contentType() const;

    /** Ends current part (if any).
        @param headers should not contain Content-Type header since it is inserted with \a contentType value
    */
    void beginPart(
        const StringType& contentType,
        const nx_http::HttpHeaders& headers,
        BufferType data);
    /** Can be called only after \a MultipartMessageBodySource::beginPart */
    void writeData(BufferType data);

    /** Writes whole part.
        Includes Content-Length header if none present in \a headers
    */
    void writeBodyPart(
        const StringType& contentType,
        const nx_http::HttpHeaders& headers,
        BufferType data);
    /** Signal end of multipart body */
    void writeEpilogue();
    /** Returns \a true after \a MultipartBodySerializer::writeEpilogue has been called */
    bool eof() const;

private:
    const StringType m_boundary;
    const StringType m_delimiter;
    std::shared_ptr<AbstractByteStreamFilter> m_outputStream;
    bool m_bodyPartStarted;
    bool m_epilogueWritten;

    void startBodyPartInternal(
        const StringType& contentType,
        const nx_http::HttpHeaders& headers,
        BufferType data,
        boost::optional<std::uint64_t> contentLength);
};

}   //namespace nx_http
