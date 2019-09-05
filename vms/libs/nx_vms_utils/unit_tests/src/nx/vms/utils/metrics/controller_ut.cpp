#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/vms/utils/metrics/system_controller.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>

#include "test_providers.h"

namespace nx::vms::utils::metrics::test {

class MetricsControllerTest: public ::testing::Test
{
public:
    MetricsControllerTest()
    {
        auto provider = std::make_unique<TestResourceController>();
        resourceProvider = provider.get();

        controller.add(std::move(provider));
        controller.start();

        for (int id = 0; id <= 2; ++id)
            resources.push_back(resourceProvider->makeResource(id));
    }

protected:
    SystemController controller;
    TestResourceController* resourceProvider = nullptr;
    std::vector<TestResource*> resources;
};

TEST_F(MetricsControllerTest, Manifest)
{
    auto systemManifest = controller.manifest();
    EXPECT_EQ(systemManifest.size(), 1);

    auto testManifest = systemManifest["tests"];
    ASSERT_EQ(testManifest.size(), 2);

    const auto group1 = testManifest[0];
    EXPECT_EQ(group1.id, "g1");
    EXPECT_EQ(group1.name, "group 1");
    ASSERT_EQ(group1.values.size(), 2);

    EXPECT_EQ(group1.values[0].id, "i");
    EXPECT_EQ(group1.values[0].name, "int parameter");
    EXPECT_EQ(group1.values[0].display, api::metrics::Displays(api::metrics::Display::both));
    EXPECT_EQ(group1.values[1].id, "t");
    EXPECT_EQ(group1.values[1].name, "text parameter");
    EXPECT_EQ(group1.values[1].display, api::metrics::Displays(api::metrics::Display::panel));

    const auto group2 = testManifest[1];
    EXPECT_EQ(group2.id, "g2");
    EXPECT_EQ(group2.name, "group 2");
    ASSERT_EQ(group2.values.size(), 2);

    EXPECT_EQ(group2.values[0].id, "i");
    EXPECT_EQ(group2.values[0].name, "int parameter");
    EXPECT_EQ(group2.values[0].display, api::metrics::Displays(api::metrics::Display::table));
    EXPECT_EQ(group2.values[1].id, "t");
    EXPECT_EQ(group2.values[1].name, "text parameter");
    EXPECT_EQ(group2.values[1].display, api::metrics::Displays(api::metrics::Display::both));
}

TEST_F(MetricsControllerTest, CurrentValues)
{
    {
        auto systemValues = controller.values();
        EXPECT_EQ(systemValues.size(), 1);

        auto testResources = systemValues["tests"];
        ASSERT_EQ(testResources.size(), 3);

        auto resource1 = testResources["R0"];
        ASSERT_EQ(resource1.size(), 2);
        {
            auto group1 = resource1["g1"];
            ASSERT_EQ(group1.size(), 2);
            EXPECT_EQ(group1["i"], 1);
            EXPECT_EQ(group1["t"], "first of 0");

            auto group2 = resource1["g2"];
            ASSERT_EQ(group2.size(), 2);
            EXPECT_EQ(group2["i"], 2);
            EXPECT_EQ(group2["t"], "second of 0");
        }

        auto resource2 = testResources["R1"];
        ASSERT_EQ(resource2.size(), 2);
        {
            auto group1 = resource2["g1"];
            ASSERT_EQ(group1.size(), 2);
            EXPECT_EQ(group1["i"], 11);
            EXPECT_EQ(group1["t"], "first of 1");

            auto group2 = resource2["g2"];
            ASSERT_EQ(group2.size(), 2);
            EXPECT_EQ(group2["i"], 12);
            EXPECT_EQ(group2["t"], "second of 1");
        }

        auto resource3 = testResources["R2"];
        ASSERT_EQ(resource3.size(), 2);
    }

    resources[0]->update("i1", 666);
    resources[1]->update("t2", "hello");

    {
        auto systemValues = controller.values();
        EXPECT_EQ(systemValues.size(), 1);

        auto testResources = systemValues["tests"];
        ASSERT_EQ(testResources.size(), 3);

        auto resource1 = testResources["R0"];
        EXPECT_EQ(resource1["g1"]["i"], 666);
        EXPECT_EQ(resource1["g1"]["t"], "first of 0");
        EXPECT_EQ(resource1["g2"]["i"], 2);
        EXPECT_EQ(resource1["g2"]["t"], "second of 0");

        auto resource2 = testResources["R1"];
        EXPECT_EQ(resource2["g1"]["i"], 11);
        EXPECT_EQ(resource2["g1"]["t"], "first of 1");
        EXPECT_EQ(resource2["g2"]["i"], 12);
        EXPECT_EQ(resource2["g2"]["t"], "hello");
    }
}

} // namespace nx::vms::utils::metrics::test
