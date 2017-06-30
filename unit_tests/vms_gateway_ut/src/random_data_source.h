#pragma once

#include <nx/network/http/abstract_msg_body_source.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

class RandomDataSource:
    public nx_http::AbstractMsgBodySource
{
public:
    RandomDataSource(nx_http::StringType contentType);

    virtual nx_http::StringType mimeType() const override;
    virtual boost::optional<uint64_t> contentLength() const override;
    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, nx_http::BufferType)> completionHandler) override;

private:
    const nx_http::StringType m_contentType;
};

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
