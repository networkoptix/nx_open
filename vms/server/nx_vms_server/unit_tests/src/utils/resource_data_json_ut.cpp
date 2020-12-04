#include <gtest/gtest.h>
#include <core/resource/param.h>
#include <core/resource_management/resource_data_pool.h>

#include <nx/vms/utils/resource_params_data.h>

TEST(QnResourceDataPoolTest, load)
{
    auto jsonData = nx::vms::utils::ResourceParamsData::load(QFile(":/resource_data.json"));
    QnResourceDataPool dataPool;
    ASSERT_TRUE(dataPool.loadData(jsonData.value));
    {
        QnResourceData data = dataPool.data("Samsung", "LNV-6011R");
        ASSERT_FALSE(data.value<bool>(ResourceDataKey::kUseMedia2ToFetchProfiles, false));
    }
    {
        QnResourceData data = dataPool.data("Samsung", "LNV-6011R", "00234");
        ASSERT_FALSE(data.value<bool>(ResourceDataKey::kUseMedia2ToFetchProfiles, false));
    }
}


