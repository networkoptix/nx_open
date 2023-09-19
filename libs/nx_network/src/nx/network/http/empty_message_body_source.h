// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_msg_body_source.h"

namespace nx::network::http {

/**
 * Should be used when HTTP request handler wishes to set Content-Length
 * to some specific value (or just omit it) and at the same time does not provide body at all.
 */
class NX_NETWORK_API EmptyMessageBodySource:
    public AbstractMsgBodySource
{
public:
    EmptyMessageBodySource(
        std::string contentType,
        const std::optional<uint64_t>& contentLength);

    virtual std::string mimeType() const override;
    virtual std::optional<uint64_t> contentLength() const override;

    /**
     * Always reports end-of-stream
     */
    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, nx::Buffer)> completionHandler) override;

private:
    std::string m_contentType;
    std::optional<uint64_t> m_contentLength;
};

} // namespace nx::network::http
