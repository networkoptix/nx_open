#include <gtest/gtest.h>

#include <nx/vms/server/metrics/controller.cpp>

#include "test_providers.h"

namespace nx::vms::server::metrics::test {

TEST(ResourceControllerTest, Main)
{
    ResourceGroupController controller(std::make_unique<TestResourceProvider>(1, 2));
    controller.startMonitoring();

    auto manifest = controller.manifest();
    EXPECT_EQ(2, manifest.size());
    EXPECT_EQ("nameParameter", manifest[0].id);
    EXPECT_EQ("numberParameter", manifest[1].id);

    auto values = controller.values();
    EXPECT_EQ(2, values.size());

    auto test1 = values[uuid(1).toSimpleString()];
    EXPECT_EQ("Test 1", test1.name);

    EXPECT_EQ(2, test1.values.size());
    EXPECT_EQ(QJsonValue(1), test1.values["numberParameter"].value);
    EXPECT_EQ(QJsonValue("Test 1"), test1.values["nameParameter"].value);

    auto test2 = values[uuid(2).toSimpleString()];
    EXPECT_EQ("Test 2", test2.name);

    EXPECT_EQ(2, test2.values.size());
    EXPECT_EQ(QJsonValue(2), test2.values["numberParameter"].value);
    EXPECT_EQ(QJsonValue("Test 2"), test2.values["nameParameter"].value);

    EXPECT_TRUE(!test1.parent.isEmpty());
    EXPECT_EQ(test1.parent, test2.parent);
}

} // namespace nx::vms::server::metrics::test
