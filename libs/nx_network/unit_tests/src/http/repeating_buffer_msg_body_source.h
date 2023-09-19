// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/aio/timer.h>
#include <nx/network/http/abstract_msg_body_source.h>

namespace nx::network::http {

class RepeatingBufferMsgBodySource:
    public nx::network::http::AbstractMsgBodySource
{
public:
    RepeatingBufferMsgBodySource(
        const std::string& mimeType,
        nx::Buffer buffer);

    virtual std::string mimeType() const override;
    virtual std::optional<uint64_t> contentLength() const override;

    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, nx::Buffer)
        > completionHandler) override;

private:
    const std::string m_mimeType;
    nx::Buffer m_buffer;
};

} // namespace nx::network::http
