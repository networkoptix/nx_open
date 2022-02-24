// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "repeating_buffer_sender.h"

#include <nx/utils/std/cpp14.h>

#include "repeating_buffer_msg_body_source.h"

namespace nx::network::http {

RepeatingBufferSender::RepeatingBufferSender(
    const std::string& mimeType,
    nx::Buffer buffer)
    :
    m_mimeType(mimeType),
    m_buffer(std::move(buffer))
{
}

void RepeatingBufferSender::processRequest(
    nx::network::http::RequestContext /*requestContext*/,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    completionHandler(
        nx::network::http::RequestResult(
            nx::network::http::StatusCode::ok,
            std::make_unique<RepeatingBufferMsgBodySource>(m_mimeType, m_buffer)));
}

} // namespace nx::network::http
