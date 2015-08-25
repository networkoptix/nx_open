/**********************************************************
* Aug 25, 2015
* a.kolesnikov
***********************************************************/

#ifndef REPEATING_BUFFER_MSG_BODY_SOURCE_H
#define REPEATING_BUFFER_MSG_BODY_SOURCE_H

#include <utils/network/http/abstract_msg_body_source.h>


class RepeatingBufferMsgBodySource
:
    public nx_http::AbstractMsgBodySource
{
public:
    RepeatingBufferMsgBodySource(
        const nx_http::StringType& mimeType,
        nx::Buffer buffer );

    //!Implementation of nx_http::AbstractMsgBodySource::mimeType
    virtual nx_http::StringType mimeType() const override;
    //!Implementation of nx_http::AbstractMsgBodySource::contentLength
    virtual boost::optional<uint64_t> contentLength() const override;
    //!Implementation of nx_http::AbstractMsgBodySource::readAsync
    virtual bool readAsync( std::function<void( SystemError::ErrorCode, nx_http::BufferType )> completionHandler ) override;

private:
    const nx_http::StringType m_mimeType;
    nx::Buffer m_buffer;
};

#endif  //REPEATING_BUFFER_MSG_BODY_SOURCE_H
