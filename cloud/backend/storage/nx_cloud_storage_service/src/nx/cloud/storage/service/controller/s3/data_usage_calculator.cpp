#include "data_usage_calculator.h"

#include <nx/cloud/aws/s3/api_client.h>
#include <nx/network/url/url_builder.h>

#include <nx/network/url/url_builder.h>

namespace nx::cloud::storage::service::controller::s3 {

DataUsageCalculator::DataUsageCalculator(const nx::network::http::Credentials& credentials):
    m_credentials(credentials)
{
}

DataUsageCalculator::~DataUsageCalculator()
{
    pleaseStopSync();
}

void DataUsageCalculator::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    for(auto& client: m_s3Clients)
        client->bindToAioThread(aioThread);
}

void DataUsageCalculator::calculate(
    std::vector<Bucket> buckets,
    nx::utils::MoveOnlyFunc<void(api::Result, int)> handler)
{
    m_handler = std::move(handler);
    dispatch(
        [this, buckets = std::move(buckets)]()
        {
            initializeS3Clients(buckets);

            for (auto& client: m_s3Clients)
            {
                client->getBucketSize(
                    [this](auto result, auto size)
                    {
                        if (result.code() != aws::ResultCode::ok)
                        {
                            return nx::utils::swapAndCall(
                                m_handler,
                                api::Result(api::ResultCode::awsApiError, result.text()),
                                0);
                        }

                        m_size += size;
                        ++m_responses;

                        if (m_handler && m_responses == (int) m_s3Clients.size())
                            nx::utils::swapAndCall(m_handler, api::Result(), m_size.load());
                    });
            }

        });
}

void DataUsageCalculator::stopWhileInAioThread()
{
    m_s3Clients.clear();
}

void DataUsageCalculator::initializeS3Clients(
    const std::vector<DataUsageCalculator::Bucket>& buckets)
{
    for (const auto& bucket: buckets)
    {
        m_s3Clients.emplace_back(
            std::make_unique<aws::s3::ApiClient>(
                std::string() /*storageClientId*/,
                bucket.region,
                bucket.url,
                m_credentials));
    }
}



} // namespace nx::cloud::storage::service::controller::s3
