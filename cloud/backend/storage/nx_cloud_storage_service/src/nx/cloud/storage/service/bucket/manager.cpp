#include "manager.h"

#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_types.h>
#include <nx/cloud/aws/api_client.h>

#include "../settings.h"

namespace nx::cloud::storage::service::bucket {

using namespace std::placeholders;

namespace {

static nx::utils::Url makeBucketUrl(const std::string& bucketName)
{
    static constexpr char kBucketUrlTemplate[] =
        "https://%1.s3.amazonaws.com";
    return lm(kBucketUrlTemplate).arg(bucketName).toQString();
}

} // namespace

Manager::Manager(const conf::Aws& /*settings*/, Database* /*database*/)/*:
    m_settings(settings),
    m_database(database)*/
{
}

void Manager::addBucket(
    const api::AddBucketRequest& /*request*/,
    const nx::utils::MoveOnlyFunc<void(api::Result, api::Error)> /*handler*/)
{
}

void Manager::listBuckets(
    nx::utils::MoveOnlyFunc<void(api::Result, std::vector<api::Bucket>)> /*handler*/)
{
}

void Manager::removeBucket(
    const std::string& /*bucketName*/,
    nx::utils::MoveOnlyFunc<void(api::Result)> /*handler*/)
{
}

//-------------------------------------------------------------------------------------------------
// ManagerFactory

ManagerFactory::ManagerFactory():
    base_type(std::bind(&ManagerFactory::defaultFactoryFunction, this, _1, _2))
{
}

ManagerFactory& ManagerFactory::instance()
{
    static ManagerFactory factory;
    return factory;
}

std::unique_ptr<AbstractManager> ManagerFactory::defaultFactoryFunction(
    const conf::Settings& settings,
    Database* database)
{
    return std::make_unique<Manager>(settings.aws(), database);
}

} // namespace nx::cloud::storage::service::bucket