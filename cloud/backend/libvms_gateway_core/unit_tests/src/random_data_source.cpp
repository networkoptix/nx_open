#include "random_data_source.h"

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

RandomDataSource::RandomDataSource(nx::network::http::StringType contentType):
    m_contentType(std::move(contentType))
{
}

nx::network::http::StringType RandomDataSource::mimeType() const
{
    return m_contentType;
}

boost::optional<uint64_t> RandomDataSource::contentLength() const
{
    return boost::none;
}

void RandomDataSource::readAsync(
    nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, nx::network::http::BufferType)> completionHandler)
{
    post(
        [completionHandler = std::move(completionHandler)]()
        {
            nx::network::http::BufferType randomData;
            randomData.resize(1024);
            std::generate(randomData.data(), randomData.data() + randomData.size(), rand);
            completionHandler(SystemError::noError, std::move(randomData));
        });
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
