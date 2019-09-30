#pragma once

#include <nx/network/aio/basic_pollable.h>

#include <vector>

#include <nx/cloud/aws/api_types.h>
#include <nx/cloud/aws/credentials.h>
#include <nx/cloud/storage/service/api/result.h>
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

        std::string toString() const;
    };

    DataUsageCalculator(const nx::cloud::aws::Credentials& credentials);
    ~DataUsageCalculator();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void calculate(
        std::vector<Bucket> bucketUrls,
        nx::utils::MoveOnlyFunc<void(api::Result, int /*bytesUsed*/)> handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    struct Context
    {
        Bucket bucket;
        std::unique_ptr<aws::s3::ApiClient> client;
        int runningSize = 0;
    };

    void initializeS3Clients(std::vector<Bucket> buckets);

    void getBucketSize(
        Context* context,
        const std::string& continuationToken);

    void calculateFailed(const aws::Result& result);

private:
    const nx::cloud::aws::Credentials& m_credentials;
    nx::utils::MoveOnlyFunc<void(api::Result, int /*bytesUsed*/)> m_handler;
    std::vector<Context> m_contexts;
    std::atomic_int m_responses = 0;
};

} // namespace storage::service::controller::s3
} // namespace nx::cloud