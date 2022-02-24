// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/abstract_msg_body_source.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

class RandomDataSource:
    public nx::network::http::AbstractMsgBodySource
{
public:
    RandomDataSource(std::string contentType);

    virtual std::string mimeType() const override;
    virtual std::optional<uint64_t> contentLength() const override;

    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, nx::Buffer)> completionHandler) override;

private:
    const std::string m_contentType;
};

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
