/**********************************************************
* Aug 24, 2015
* a.kolesnikov
***********************************************************/

#include "repeating_buffer_sender.h"

#include <utils/common/cpp14.h>

#include "repeating_buffer_msg_body_source.h"


RepeatingBufferSender::RepeatingBufferSender(
    const nx_http::StringType& mimeType,
    nx::Buffer buffer )
:
    m_mimeType( mimeType ),
    m_buffer( std::move( buffer ) )
{
}

void RepeatingBufferSender::processRequest(
    const nx_http::HttpServerConnection& /*connection*/,
    stree::ResourceContainer&& /*authInfo*/,
    const nx_http::Request& /*request*/,
    nx_http::Response* const /*response*/,
    std::function<void(
        const nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler )
{
    completionHandler(
        nx_http::StatusCode::notImplemented,
        std::make_unique<RepeatingBufferMsgBodySource>( m_mimeType, m_buffer ) );
}
