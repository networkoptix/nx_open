
#include <gtest/gtest.h>

#include <nx/cloud/db/api/cloud_nonce.h>

namespace nx::cloud::db {

TEST(CloudNonce, general)
{
    std::string systemId = "blablabla";
    for (int i = 0; i < 1000; ++i)
    {
        const auto cloudNonceBase = api::generateCloudNonceBase(systemId);
        ASSERT_TRUE(api::isValidCloudNonceBase(cloudNonceBase, systemId));
    }
}

} // namespace nx::cloud::db
