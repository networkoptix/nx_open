#include "s3_bucket.h"

#include <nx/cloud/storage/service/utils.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_types.h>

namespace nx::cloud::storage::service::test {

using namespace nx::network;

S3Bucket::S3Bucket(std::string location, std::string name, bool local):
    base_type(location),
    m_name(name)
{
    if (!local)
        return;

    NX_ASSERT(bindAndListen(SocketAddress::anyPrivateAddress));
    SocketGlobals::addressResolver().addFixedAddress(hostName(), serverAddress());
}

S3Bucket::~S3Bucket()
{
    SocketGlobals::addressResolver().removeFixedAddress(hostName());
}

const std::string& S3Bucket::name() const
{
    return m_name;
}

nx::utils::Url S3Bucket::url() const
{
    return service::utils::bucketUrl(m_name);
}

QString S3Bucket::hostName() const
{
    return lm("%1.s3.amazonaws.com").arg(m_name);
}

} // namespace nx::cloud::storage::service::test