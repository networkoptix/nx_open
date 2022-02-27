// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_msg_body_source.h"

namespace nx::network::http {

/**
 * Provides an in-memory buffer as a HTTP message body.
 */
class NX_NETWORK_API BufferSource:
    public AbstractMsgBodySourceWithCache
{
public:
    BufferSource(std::string mimeType, nx::Buffer msgBody);

    virtual std::string mimeType() const override;
    virtual std::optional<uint64_t> contentLength() const override;

    /**
     * Provides the whole buffer as the read result.
     */
    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, nx::Buffer)
        > completionHandler) override;

    virtual std::optional<std::tuple<SystemError::ErrorCode, nx::Buffer>> peek() override;
    virtual void cancelRead() override;

private:
    const std::string m_mimeType;
    nx::Buffer m_msgBody;
};

} // namespace nx::network::http
