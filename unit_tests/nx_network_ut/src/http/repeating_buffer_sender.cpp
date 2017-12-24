#include "repeating_buffer_sender.h"

#include <nx/utils/std/cpp14.h>

#include "repeating_buffer_msg_body_source.h"

RepeatingBufferSender::RepeatingBufferSender(
    const nx_http::StringType& mimeType,
    nx::Buffer buffer)
    :
    m_mimeType(mimeType),
    m_buffer(std::move(buffer))
{
}

void RepeatingBufferSender::processRequest(
    nx_http::HttpServerConnection* const /*connection*/,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    nx_http::Request /*request*/,
    nx_http::Response* const /*response*/,
    nx_http::RequestProcessedHandler completionHandler)
{
    completionHandler(
        nx_http::RequestResult(
            nx_http::StatusCode::ok,
            std::make_unique<RepeatingBufferMsgBodySource>(m_mimeType, m_buffer)));
}
