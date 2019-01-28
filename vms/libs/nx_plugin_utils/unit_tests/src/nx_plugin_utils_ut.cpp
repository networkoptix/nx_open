#include <nx/vms_server_plugins/utils/uuid.h>

#include <gtest/gtest.h>

namespace nx {
namespace media {
namespace test {

TEST(NxPluginUtils, uuid)
{
    static constexpr nx::sdk::Uuid kSdkUuid{
        0xd4,0xbb,0x55,0xa4,0xa8,0x7a,0x41,0x99,0xbb,0xd1,0x80,0x00,0xa4,0x80,0xa5,0xad};

    const QnUuid qnUuid = nx::vms_server_plugins::utils::fromSdkUuidToQnUuid(kSdkUuid);
    ASSERT_EQ(QByteArray((const char*) &kSdkUuid[0], nx::sdk::Uuid::kSize), qnUuid.toRfc4122());

    const nx::sdk::Uuid sdkUuid = nx::vms_server_plugins::utils::fromQnUuidToSdkUuid(qnUuid);
    ASSERT_EQ(kSdkUuid, sdkUuid);
}

} // namespace test
} // namespace media
} // namespace nx
