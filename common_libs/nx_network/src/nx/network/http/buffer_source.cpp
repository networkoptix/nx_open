/**********************************************************
* 20 may 2015
* a.kolesnikov
***********************************************************/

#include "buffer_source.h"


namespace nx_http
{
    BufferSource::BufferSource( StringType mimeType, BufferType msgBody )
    :
        m_mimeType( std::move( mimeType ) ),
        m_msgBody( std::move( msgBody ) )
    {
    }

    StringType BufferSource::mimeType() const
    {
        return m_mimeType;
    }

    boost::optional<uint64_t> BufferSource::contentLength() const
    {
        return boost::optional<uint64_t>( m_msgBody.size() );
    }

    bool BufferSource::readAsync( std::function<void( SystemError::ErrorCode, BufferType )> completionHandler )
    {
        auto outMsgBody = std::move( m_msgBody );
        m_msgBody = BufferType();   //moving to valid state
        completionHandler( SystemError::noError, std::move( outMsgBody ) );
        return true;
    }
}
