#include "data_usage_calculator.h"

#include <nx/cloud/aws/s3/api_client.h>
#include <nx/network/url/url_builder.h>

#include "nx/cloud/storage/service/utils.h"
#include "nx/cloud/storage/service/controller/utils.h"

namespace nx::cloud::storage::service::controller::s3 {

namespace {

int calculateSize(const aws::s3::ListBucketResult& result)
{
    int size = 0;
    for (const auto& content : result.contents)
        size += content.size;
    return size;
}

} // namespace

std::string DataUsageCalculator::Bucket::toString() const
{
    return std::string("{region: ") + region + ", url: " + url.toStdString() + "}";
}

DataUsageCalculator::DataUsageCalculator(const nx::cloud::aws::Credentials& credentials):
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

    for(auto& context: m_contexts)
        context.client->bindToAioThread(aioThread);
}

void DataUsageCalculator::calculate(
    std::vector<Bucket> buckets,
    nx::utils::MoveOnlyFunc<void(api::Result, int)> handler)
{
    m_handler = std::move(handler);
    dispatch(
        [this, buckets = std::move(buckets)]()
        {
            initializeS3Clients(std::move(buckets));

            for (auto& context: m_contexts)
                getBucketSize(&context, std::string());
        });
}

void DataUsageCalculator::stopWhileInAioThread()
{
    m_contexts.clear();
}

void DataUsageCalculator::initializeS3Clients(
    std::vector<DataUsageCalculator::Bucket> buckets)
{
    for (auto&& bucket: buckets)
    {
        m_contexts.emplace_back(Context{});
        m_contexts.back().bucket = std::move(bucket);
        m_contexts.back().client =
            std::make_unique<aws::s3::ApiClient>(
                m_contexts.back().bucket.region,
                m_contexts.back().bucket.url,
                m_credentials);
    }
}

void DataUsageCalculator::getBucketSize(
    Context* context,
    const std::string& continuationToken)
{
    aws::s3::ListBucketRequest request;
    request.continuationToken = continuationToken;
    request.maxKeys = 1000;
    request.prefix = service::utils::storageFolder(context->bucket.url);
    context->client->listBucket(
        request,
        [this, context](auto result, auto listBucketResult)
        {
            if (!result.ok())
                return calculateFailed(result);

            context->runningSize += calculateSize(listBucketResult);

            if (listBucketResult.isTruncated)
            {
                if (listBucketResult.nextContinuationToken.empty())
                {
                    return calculateFailed(
                        aws::Result(
                            aws::ResultCode::error,
                            "Expected a coninuation token but didn't receive one"));
                }
                return getBucketSize(context, listBucketResult.nextContinuationToken);
            }

            if (++m_responses < (int) m_contexts.size())
                return;

            int totalUsage = 0;
            for (const auto& context : m_contexts)
                totalUsage += context.runningSize;

            if (m_handler)
                nx::utils::swapAndCall(m_handler, api::Result(), totalUsage);
        });
}

void DataUsageCalculator::calculateFailed(
    const aws::Result& result)
{
    NX_VERBOSE(this, "Calculation failed: %1, %2",
        aws::toString(result.code()).data(), result.text());
    if (m_handler)
        nx::utils::swapAndCall(m_handler, utils::toResult(result), 0);
}

} // namespace nx::cloud::storage::service::controller::s3
