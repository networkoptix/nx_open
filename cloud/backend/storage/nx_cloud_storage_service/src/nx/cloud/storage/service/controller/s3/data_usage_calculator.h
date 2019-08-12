#pragma once

#include <nx/network/aio/basic_pollable.h>

#include <vector>

#include <nx/cloud/storage/service/api/result.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/url.h>
#include <nx/utils/move_only_func.h>

namespace nx::cloud {

namespace aws::s3 { class ApiClient; }

namespace storage::service::controller::s3 {

class DataUsageCalculator:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    struct Bucket
    {
        std::string region;
        nx::utils::Url url;
    };

    DataUsageCalculator(const nx::network::http::Credentials& credentials);
    ~DataUsageCalculator();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void calculate(
        std::vector<Bucket> bucketUrls,
        nx::utils::MoveOnlyFunc<void(api::Result, int /*bytesUsed*/)> handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    void initializeS3Clients(const std::vector<Bucket>& buckets);

private:
    network::http::Credentials m_credentials;
    nx::utils::MoveOnlyFunc<void(api::Result, int /*bytesUsed*/)> m_handler;
    std::vector<std::unique_ptr<aws::s3::ApiClient>> m_s3Clients;
    std::atomic_int m_size = 0;
    std::atomic_int m_responses = 0;
};

} // namespace storage::service::controller::s3
} // namespace nx::cloud