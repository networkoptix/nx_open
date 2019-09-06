#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/vms/utils/metrics/system_controller.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>

#include "test_providers.h"

namespace nx::vms::utils::metrics::test {

static const QByteArray kRules(R"json({
    "tests": {
        "g1": {
            "c": { "calculate": "const hello" },
            "ip": {
                "name": "i plus",
                "display": "panel",
                "calculate": "add %i 1",
                "insert": "i",
                "alarms": [{
                    "level": "warning",
                    "condition": "greaterThen %i 100",
                    "text": "i = %i, ip = %ip"
                },{
                    "level": "danger",
                    "condition": "lessThen %i 0",
                    "text": "i = %i, ip = %ip"
                }]
            }
        }
    }
})json");

class MetricsControllerTest: public ::testing::Test
{
public:
    MetricsControllerTest()
    {
        auto provider = std::make_unique<TestResourceController>();
        const auto resourceProvider = provider.get();

        m_systemController.add(std::move(provider));
        m_systemController.start();

        for (int id = 0; id <= 2; ++id)
            m_resources.push_back(resourceProvider->makeResource(id));
    }

    void setRules()
    {
        api::metrics::SystemRules rules;
        NX_CRITICAL(QJson::deserialize(kRules, &rules));
        m_systemController.setRules(std::move(rules));
    }

    void testManifest(bool includeRules)
    {
        auto systemManifest = m_systemController.manifest();
        EXPECT_EQ(systemManifest.size(), 1);

        auto testManifest = systemManifest["tests"];
        ASSERT_EQ(testManifest.size(), 2);

        const auto group1 = testManifest[0];
        EXPECT_EQ(group1.id, "g1");
        EXPECT_EQ(group1.name, "group 1");
        ASSERT_EQ(group1.values.size(), includeRules ? 4 : 2);

        int b = 0;
        if (includeRules)
        {
            b = 1;
            EXPECT_EQ(group1.values[0].id, "ip");
            EXPECT_EQ(group1.values[0].name, "i plus");
            EXPECT_EQ(group1.values[0].display, api::metrics::Displays(api::metrics::Display::panel));
            EXPECT_EQ(group1.values[3].id, "c");
            EXPECT_EQ(group1.values[3].name, "C");
            EXPECT_EQ(group1.values[3].display, api::metrics::Displays(api::metrics::Display::none));
        }

        EXPECT_EQ(group1.values[b + 0].id, "i");
        EXPECT_EQ(group1.values[b + 0].name, "int parameter");
        EXPECT_EQ(group1.values[b + 0].display, api::metrics::Displays(api::metrics::Display::both));
        EXPECT_EQ(group1.values[b + 1].id, "t");
        EXPECT_EQ(group1.values[b + 1].name, "text parameter");
        EXPECT_EQ(group1.values[b + 1].display, api::metrics::Displays(api::metrics::Display::panel));

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

    void testValues(bool includeRules)
    {
        {
            auto systemValues = m_systemController.values();
            EXPECT_EQ(systemValues.size(), 1);

            auto testResources = systemValues["tests"];
            ASSERT_EQ(testResources.size(), 3);

            auto resource1 = testResources["R0"];
            ASSERT_EQ(resource1.size(), 2);
            {
                auto group1 = resource1["g1"];
                ASSERT_EQ(group1.size(), includeRules ? 4 : 2);
                EXPECT_EQ(group1["i"], 1);
                EXPECT_EQ(group1["t"], "first of 0");
                if (includeRules)
                {
                    EXPECT_EQ(group1["c"], "hello");
                    EXPECT_EQ(group1["ip"], 2);
                }

                auto group2 = resource1["g2"];
                ASSERT_EQ(group2.size(), 2);
                EXPECT_EQ(group2["i"], 2);
                EXPECT_EQ(group2["t"], "second of 0");
            }

            auto resource2 = testResources["R1"];
            ASSERT_EQ(resource2.size(), 2);
            {
                auto group1 = resource2["g1"];
                ASSERT_EQ(group1.size(), includeRules ? 4 : 2);
                EXPECT_EQ(group1["i"], 11);
                EXPECT_EQ(group1["t"], "first of 1");
                if (includeRules)
                {
                    EXPECT_EQ(group1["c"], "hello");
                    EXPECT_EQ(group1["ip"], 12);
                }

                auto group2 = resource2["g2"];
                EXPECT_EQ(group2["i"], 12);
                EXPECT_EQ(group2["t"], "second of 1");
            }

            auto resource3 = testResources["R2"];
            ASSERT_EQ(resource3.size(), 2);
        }

        m_resources[0]->update("i1", 666);
        m_resources[1]->update("t2", "hello");

        {
            auto systemValues = m_systemController.values();
            EXPECT_EQ(systemValues.size(), 1);

            auto testResources = systemValues["tests"];
            ASSERT_EQ(testResources.size(), 3);

            auto resource1 = testResources["R0"];
            EXPECT_EQ(resource1["g1"]["i"], 666);
            EXPECT_EQ(resource1["g1"]["t"], "first of 0");
            EXPECT_EQ(resource1["g2"]["i"], 2);
            EXPECT_EQ(resource1["g2"]["t"], "second of 0");
            if (includeRules)
            {
                EXPECT_EQ(resource1["g1"]["c"], "hello");
                EXPECT_EQ(resource1["g1"]["ip"], 667);
            }

            auto resource2 = testResources["R1"];
            EXPECT_EQ(resource2["g1"]["i"], 11);
            EXPECT_EQ(resource2["g1"]["t"], "first of 1");
            EXPECT_EQ(resource2["g2"]["i"], 12);
            EXPECT_EQ(resource2["g2"]["t"], "hello");
            if (includeRules)
            {
                EXPECT_EQ(resource2["g1"]["c"], "hello");
                EXPECT_EQ(resource2["g1"]["ip"], 12);
            }
        }
    }

protected:
    SystemController m_systemController;
    std::vector<TestResource*> m_resources;
};

TEST_F(MetricsControllerTest, Manifest)
{
    testManifest(/*includeRules*/ false);
}

TEST_F(MetricsControllerTest, ManifestWithRules)
{
    setRules();
    testManifest(/*includeRules*/ true);
}

TEST_F(MetricsControllerTest, Values)
{
    testValues(/*includeRules*/ false);
}

TEST_F(MetricsControllerTest, ValuesWithRules)
{
    setRules();
    testValues(/*includeRules*/ true);
}

TEST_F(MetricsControllerTest, Alarms)
{
    {
        const auto alarms = m_systemController.alarms();
        ASSERT_EQ(alarms.size(), 0);
    }

    setRules();
    {
        const auto alarms = m_systemController.alarms();
        ASSERT_EQ(alarms.size(), 0);
    }

    m_resources[0]->update("i1", 150);
    m_resources[2]->update("i1", -2);
    {
        const auto alarms = m_systemController.alarms();
        ASSERT_EQ(alarms.size(), 2);

        EXPECT_EQ(alarms[0].resource, "R0");
        EXPECT_EQ(alarms[0].parameter, "g1.ip");
        EXPECT_EQ(alarms[0].level, api::metrics::AlarmLevel::warning);
        EXPECT_EQ(alarms[0].text, "i = 150, ip = 151");

        EXPECT_EQ(alarms[1].resource, "R2");
        EXPECT_EQ(alarms[1].parameter, "g1.ip");
        EXPECT_EQ(alarms[1].level, api::metrics::AlarmLevel::danger);
        EXPECT_EQ(alarms[1].text, "i = -2, ip = -1");
    }

    m_resources[0]->update("i1", 7);
    m_resources[1]->update("i1", 500);
    m_resources[2]->update("i1", 8);
    {
        const auto alarms = m_systemController.alarms();
        ASSERT_EQ(alarms.size(), 1);

        EXPECT_EQ(alarms[0].resource, "R1");
        EXPECT_EQ(alarms[0].parameter, "g1.ip");
        EXPECT_EQ(alarms[0].level, api::metrics::AlarmLevel::warning);
        EXPECT_EQ(alarms[0].text, "i = 500, ip = 501");
    }
}

} // namespace nx::vms::utils::metrics::test
