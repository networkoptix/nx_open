#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/vms/utils/metrics/system_controller.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>

#include "test_providers.h"

namespace nx::vms::utils::metrics::test {

static const QByteArray kRules(R"json({
    "tests": {
        "g1": {
            "name": "group 1",
            "values": {
                "i": {
                    "name": "int parameter",
                    "display": "panel|table"
                },
                "t": {
                    "name": "text parameter",
                    "display": "panel"
                },
                "c": {
                    "calculate": "const hello"
                },
                "ip": {
                    "name": "i plus",
                    "display": "panel",
                    "calculate": "add %i 1",
                    "insert": "i",
                    "alarms": [{
                        "level": "warning",
                        "condition": "greaterThen %i 100",
                        "text": "i = %i (>100), ip = %ip"
                    },{
                        "level": "danger",
                        "condition": "lessThen %i 0",
                        "text": "i = %i (<0), ip = %ip"
                    }]
                }
            }
        },
        "g2": {
            "name": "group 2",
            "values": {
                "i": {
                    "name": "int parameter",
                    "display": "table",
                    "alarms": [{
                        "level": "warning",
                        "condition": "greaterThen %i 30",
                        "text": "i is %i (>30)"
                    }]
                },
                "im": {
                    "display": "table",
                    "calculate": "sub %i 1"
                },
                "t": {
                    "display": "table|panel"
                }
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
        const api::metrics::Displays displayNone(api::metrics::Display::none);
        const api::metrics::Displays displayPanel(api::metrics::Display::panel);
        const api::metrics::Displays displayTable(api::metrics::Display::table);
        const api::metrics::Displays displayBoth = displayPanel | displayTable;

        auto systemManifest = m_systemController.manifest();
        EXPECT_EQ(systemManifest.size(), 1);

        auto testManifest = systemManifest["tests"];
        ASSERT_EQ(testManifest.size(), 2);

        const auto group1 = testManifest[0];
        EXPECT_EQ(group1.id, "g1");
        EXPECT_EQ(group1.name, includeRules ? "group 1" : "G1");
        ASSERT_EQ(group1.values.size(), includeRules ? 4 : 2);

        int b = 0;
        if (includeRules)
        {
            b = 1;
            EXPECT_EQ(group1.values[0].id, "ip");
            EXPECT_EQ(group1.values[0].name, "i plus");
            EXPECT_EQ(group1.values[0].display, displayPanel);
            EXPECT_EQ(group1.values[3].id, "c");
            EXPECT_EQ(group1.values[3].name, "C");
            EXPECT_EQ(group1.values[3].display, displayNone);
        }

        EXPECT_EQ(group1.values[b + 0].id, "i");
        EXPECT_EQ(group1.values[b + 0].name, includeRules ? "int parameter" : "I");
        EXPECT_EQ(group1.values[b + 0].display, includeRules ? displayBoth : displayNone);
        EXPECT_EQ(group1.values[b + 1].id, "t");
        EXPECT_EQ(group1.values[b + 1].name, includeRules ? "text parameter" : "T");
        EXPECT_EQ(group1.values[b + 1].display, includeRules ? displayPanel : displayNone);

        const auto group2 = testManifest[1];
        EXPECT_EQ(group2.id, "g2");
        EXPECT_EQ(group2.name, includeRules ? "group 2" : "G2");
        ASSERT_EQ(group2.values.size(), includeRules? 3 : 2);

        EXPECT_EQ(group2.values[0].id, "i");
        EXPECT_EQ(group2.values[0].name, includeRules ? "int parameter" : "I");
        EXPECT_EQ(group2.values[0].display, includeRules ? displayTable : displayNone);
        EXPECT_EQ(group2.values[1].id, "t");
        EXPECT_EQ(group2.values[1].name, "T");
        EXPECT_EQ(group2.values[1].display, includeRules ? displayBoth : displayNone);

        if (includeRules)
        {
            EXPECT_EQ(group2.values[2].id, "im");
            EXPECT_EQ(group2.values[2].name, "Im");
            EXPECT_EQ(group2.values[2].display, displayTable);
        }
    }

    void testValues(Scope scope, bool includeRules)
    {
        for (const auto& resource: m_resources)
            resource->setDefaults();
        {
            auto systemValues = m_systemController.values(scope);
            EXPECT_EQ(systemValues.size(), 1);

            auto testResources = systemValues["tests"];
            ASSERT_EQ(testResources.size(), (scope == Scope::system) ? 3 : 2);

            auto local1 = testResources["R0"];
            ASSERT_EQ(local1.size(), 2);
            {
                auto group1 = local1["g1"];
                ASSERT_EQ(group1.size(), includeRules ? ((scope == Scope::system) ? 4 : 3) : 2);
                EXPECT_EQ(group1["i"], 1);
                EXPECT_EQ(group1["t"], "first of 0");
                if (includeRules)
                {
                    EXPECT_EQ(group1["ip"], 2);
                    if (scope == Scope::system)
                    {
                        EXPECT_EQ(group1["c"], "hello");
                    }
                }

                auto group2 = local1["g2"];
                ASSERT_EQ(group2.size(), includeRules ? 3 : 2);
                EXPECT_EQ(group2["i"], 2);
                EXPECT_EQ(group2["t"], "second of 0");
                if (includeRules)
                {
                    EXPECT_EQ(group2["im"], 1);
                }
            }

            auto local2 = testResources["R2"];
            ASSERT_EQ(local2.size(), 2);
            {
                auto group1 = local2["g1"];
                ASSERT_EQ(group1.size(), includeRules ? ((scope == Scope::system) ? 4 : 3) : 2);
                EXPECT_EQ(group1["i"], 21);
                EXPECT_EQ(group1["t"], "first of 2");
                if (includeRules)
                {
                    EXPECT_EQ(group1["ip"], 22);
                    if (scope == Scope::system)
                    {
                        EXPECT_EQ(group1["c"], "hello");
                    }
                }

                auto group2 = local2["g2"];
                EXPECT_EQ(group2["i"], 22);
                EXPECT_EQ(group2["t"], "second of 2");
                if (includeRules)
                {
                    EXPECT_EQ(group2["im"], 21);
                }
            }

            if (scope == Scope::system)
            {
                auto remote = testResources["R1"];
                ASSERT_EQ(remote.size(), 1);

                auto group2 = remote["g2"];
                ASSERT_EQ(group2.size(), includeRules ? 3 : 2);
                EXPECT_EQ(group2["i"], 12);
                EXPECT_EQ(group2["t"], "second of 1");
            }
        }

        m_resources[0]->update("i1", 666);
        m_resources[1]->update("t2", "hello");
        {
            auto systemValues = m_systemController.values(scope);
            EXPECT_EQ(systemValues.size(), 1);

            auto testResources = systemValues["tests"];
            ASSERT_EQ(testResources.size(), scope == Scope::system ? 3 : 2);

            auto local1 = testResources["R0"];
            EXPECT_EQ(local1["g1"]["i"], 666);
            EXPECT_EQ(local1["g1"]["t"], "first of 0");
            EXPECT_EQ(local1["g2"]["i"], 2);
            EXPECT_EQ(local1["g2"]["t"], "second of 0");
            if (includeRules)
            {
                EXPECT_EQ(local1["g1"]["ip"], 667);
                if (scope == Scope::system)
                {
                    EXPECT_EQ(local1["g1"]["c"], "hello");
                }
            }

            auto local2 = testResources["R2"];
            EXPECT_EQ(local2["g1"]["i"], 21);
            EXPECT_EQ(local2["g1"]["t"], "first of 2");
            EXPECT_EQ(local2["g2"]["i"], 22);
            EXPECT_EQ(local2["g2"]["t"], "second of 2");
            if (includeRules)
            {
                EXPECT_EQ(local2["g1"]["ip"], 22);
                if (scope == Scope::system)
                {
                    EXPECT_EQ(local2["g1"]["c"], "hello");
                }
            }

            if (scope == Scope::system)
            {
                auto remote = testResources["R1"];
                ASSERT_EQ(remote.size(), 1);

                auto group2 = remote["g2"];
                ASSERT_EQ(group2.size(), includeRules ? 3 : 2);
                EXPECT_EQ(group2["i"], 12);
                EXPECT_EQ(group2["t"], "hello");
                if (includeRules)
                {
                    EXPECT_EQ(group2["im"], 11);
                }
            }
        }
    }

    void testAlarms(Scope scope)
    {
        #define EXPECT_ALARM(ALARM, RESOURCE, PARAMETER, LEVEL, TEXT) \
        { \
            const auto& alarm = ALARM; \
            EXPECT_EQ(alarm.resource, RESOURCE); \
            EXPECT_EQ(alarm.parameter, PARAMETER); \
            EXPECT_EQ(alarm.level, api::metrics::AlarmLevel::LEVEL); \
            EXPECT_EQ(alarm.text, TEXT); \
        }

        for (const auto& resource: m_resources)
            resource->setDefaults();
        {
            const auto alarms = m_systemController.alarms(scope);
            ASSERT_EQ(alarms.size(), 0);
        }

        m_resources[0]->update("i1", 150);
        m_resources[0]->update("i2", 50);
        m_resources[1]->update("i2", 70);
        m_resources[2]->update("i1", -2);
        {
            const auto alarms = m_systemController.alarms(scope);
            ASSERT_EQ(alarms.size(), (scope == Scope::system) ? 4 : 3);

            EXPECT_ALARM(alarms[0], "R0", "g1.ip", warning, "i = 150 (>100), ip = 151");
            EXPECT_ALARM(alarms[1], "R0", "g2.i", warning, "i is 50 (>30)");
            if (scope == Scope::system)
            {
                EXPECT_ALARM(alarms[2], "R1", "g2.i", warning, "i is 70 (>30)");
                EXPECT_ALARM(alarms[3], "R2", "g1.ip", danger, "i = -2 (<0), ip = -1");
            }
            else
            {
                EXPECT_ALARM(alarms[2], "R2", "g1.ip", danger, "i = -2 (<0), ip = -1");
            }
        }

        for (const auto& resource: m_resources)
            resource->setDefaults();
        m_resources[2]->update("i2", 50);
        {
            const auto alarms = m_systemController.alarms(scope);
            ASSERT_EQ(alarms.size(), 1);
            EXPECT_ALARM(alarms[0], "R2", "g2.i", warning, "i is 50 (>30)");
        }

        #undef EXPECT_ALARM
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

TEST_F(MetricsControllerTest, ManifestScenario)
{
    testManifest(/*includeRules*/ false);
    setRules();
    testManifest(/*includeRules*/ true);
}

TEST_F(MetricsControllerTest, LocalValues)
{
    testValues(Scope::local, /*includeRules*/ false);
}

TEST_F(MetricsControllerTest, LocalValuesWithRules)
{
    setRules();
    testValues(Scope::local, /*includeRules*/ true);
}

TEST_F(MetricsControllerTest, SystemValues)
{
    testValues(Scope::system, /*includeRules*/ false);
}

TEST_F(MetricsControllerTest, SystemValuesWithRules)
{
    setRules();
    testValues(Scope::system, /*includeRules*/ true);
}

TEST_F(MetricsControllerTest, ValuesScenario)
{
    testValues(Scope::local, /*includeRules*/ false);
    testValues(Scope::system, /*includeRules*/ false);
    setRules();
    testValues(Scope::local, /*includeRules*/ true);
    testValues(Scope::system, /*includeRules*/ true);
}

TEST_F(MetricsControllerTest, LocalAlarms)
{
    ASSERT_EQ(m_systemController.alarms(Scope::local).size(), 0);
    setRules();
    testAlarms(Scope::local);
}

TEST_F(MetricsControllerTest, SystemAlarms)
{
    ASSERT_EQ(m_systemController.alarms(Scope::system).size(), 0);
    setRules();
    testAlarms(Scope::system);
}

TEST_F(MetricsControllerTest, AlarmsScenario)
{
    ASSERT_EQ(m_systemController.alarms(Scope::local).size(), 0);
    ASSERT_EQ(m_systemController.alarms(Scope::system).size(), 0);
    setRules();
    testAlarms(Scope::local);
    testAlarms(Scope::system);
}

} // namespace nx::vms::utils::metrics::test
