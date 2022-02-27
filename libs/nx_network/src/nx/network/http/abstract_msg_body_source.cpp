// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_msg_body_source.h"

namespace nx::network::http {

void AbstractMsgBodySourceWithCache::readSomeAsync(
    nx::Buffer* const buffer,
    IoCompletionHandler handler)
{
    readAsync(
        [userBuffer = buffer, handler = std::move(handler)](
            auto resultCode, auto buffer)
        {
            *userBuffer = std::move(buffer);
            handler(resultCode, userBuffer->size());
        });
}

} // namespace nx::network::http
