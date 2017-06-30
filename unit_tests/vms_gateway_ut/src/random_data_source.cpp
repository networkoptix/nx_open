#include "random_data_source.h"

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

RandomDataSource::RandomDataSource(nx_http::StringType contentType):
    m_contentType(std::move(contentType))
{
}

nx_http::StringType RandomDataSource::mimeType() const
{
    return m_contentType;
}

boost::optional<uint64_t> RandomDataSource::contentLength() const
{
    return boost::none;
}

void RandomDataSource::readAsync(
    nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, nx_http::BufferType)> completionHandler)
{
    post(
        [completionHandler = std::move(completionHandler)]()
        {
            nx_http::BufferType randomData;
            randomData.resize(1024);
            std::generate(randomData.data(), randomData.data() + randomData.size(), rand);
            completionHandler(SystemError::noError, std::move(randomData));
        });
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
