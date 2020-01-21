#include <gtest/gtest.h>

#include <plugins/resource/onvif/onvif_resource_information_fetcher.h>
#include <core/resource_management/resource_data_pool.h>

namespace nx::vms::server::search::test {

TEST(OnvifResourceSearcher, ignoreOnvifVendors)
{
    QnResourceDataPool dataPool;
    ASSERT_FALSE(OnvifResourceInformationFetcher::ignoreCamera(&dataPool, "123", "123"));
    ASSERT_TRUE(OnvifResourceInformationFetcher::ignoreCamera(&dataPool, "Hanwha", "model1"));
    ASSERT_TRUE(OnvifResourceInformationFetcher::ignoreCamera(&dataPool, "HanwhA1", "model1"));
    ASSERT_TRUE(OnvifResourceInformationFetcher::ignoreCamera(&dataPool, "model1", "HanwhA1"));
}

} // namespace nx::vms::server::search::test
