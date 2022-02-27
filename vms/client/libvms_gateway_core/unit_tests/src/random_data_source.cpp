// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "random_data_source.h"

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

RandomDataSource::RandomDataSource(std::string contentType):
    m_contentType(std::move(contentType))
{
}

std::string RandomDataSource::mimeType() const
{
    return m_contentType;
}

std::optional<uint64_t> RandomDataSource::contentLength() const
{
    return std::nullopt;
}

void RandomDataSource::readAsync(
    nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, nx::Buffer)> completionHandler)
{
    post(
        [completionHandler = std::move(completionHandler)]()
        {
            nx::Buffer randomData;
            randomData.resize(1024);
            std::generate(randomData.data(), randomData.data() + randomData.size(), rand);
            completionHandler(SystemError::noError, std::move(randomData));
        });
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
