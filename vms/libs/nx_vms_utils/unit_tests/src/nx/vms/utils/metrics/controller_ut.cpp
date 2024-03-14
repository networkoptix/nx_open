// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>
#include <nx/vms/utils/metrics/system_controller.h>

#include "test_providers.h"

namespace nx::vms::utils::metrics::test {

using Value = api::metrics::Value;

static const QByteArray kRules(R"json({
    "tests": {
        "name": "Test Resources",
        "resource": "Test",
        "values": {
            "g1": {
                "name": "group 1",
                "values": {
                    "i": {
                        "name": "int parameter",
                        "description": "integer parameter",
                        "display": "panel|table",
                        "format": "KB"
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
                            "condition": "greaterThan %i 100",
                            "text": "i = %i (>100), ip = %ip"
                        },{
                            "level": "error",
                            "condition": "lessThan %i 0",
                            "text": "i = %i (<0), ip = %ip"
                        }]
                    }
                }
            },
            "g2": {
                "values": {
                    "i": {
                        "display": "table",
                        "alarms": [{
                            "level": "warning",
                            "condition": "greaterThan %i 30",
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
        api::metrics::SiteRules rules;
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

        auto testManifest = systemManifest[0];
        EXPECT_EQ(testManifest.id, "tests");
        EXPECT_EQ(testManifest.name, includeRules ? "Test Resources" : "Tests");
        EXPECT_EQ(testManifest.resource, includeRules ? "Test" : "");
        ASSERT_EQ(testManifest.values.size(), 3);

        const auto group0 = testManifest.values[0];
        EXPECT_EQ(group0.id, "_");
        EXPECT_EQ(group0.name, "");
        ASSERT_EQ(group0.values.size(), 1);
        EXPECT_EQ(group0.values[0].id, "name");
        EXPECT_EQ(group0.values[0].name, "Name");
        EXPECT_EQ(group0.values[0].display, displayNone);

        const auto group1 = testManifest.values[1];
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
        EXPECT_EQ(group1.values[b + 0].description, includeRules ? "integer parameter" : "");
        EXPECT_EQ(group1.values[b + 0].display, includeRules ? displayBoth : displayNone);
        EXPECT_EQ(group1.values[b + 1].id, "t");
        EXPECT_EQ(group1.values[b + 1].name, includeRules ? "text parameter" : "T");
        EXPECT_EQ(group1.values[b + 1].display, includeRules ? displayPanel : displayNone);

        const auto group2 = testManifest.values[2];
        EXPECT_EQ(group2.id, "g2");
        EXPECT_EQ(group2.name, "G2");
        ASSERT_EQ(group2.values.size(), includeRules? 3 : 2);

        EXPECT_EQ(group2.values[0].id, "i");
        EXPECT_EQ(group2.values[0].name, "I");
        EXPECT_EQ(group2.values[0].description, "");
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

    void testValues(Scope scope, bool includeRules, bool formatted)
    {
        for (const auto& resource: m_resources)
            resource->setDefaults();
        {
            auto systemValues = m_systemController.values(scope, formatted);
            EXPECT_EQ(systemValues.size(), 1);

            auto testResources = systemValues["tests"];
            ASSERT_EQ(testResources.size(), (scope == Scope::site) ? 3 : 2);

            auto local1 = testResources["R0"];
            ASSERT_EQ(local1.size(), (scope == Scope::site) ? 3 : 2);
            {
                EXPECT_EQ(local1["_"]["name"], "TR0");

                auto group1 = local1["g1"];
                ASSERT_EQ(group1.size(), includeRules ? ((scope == Scope::site) ? 4 : 3) : 2);
                EXPECT_EQ(group1["i"], formatted ? Value("0 KB") : Value(1));
                EXPECT_EQ(group1["t"], "first of 0");
                if (includeRules)
                {
                    EXPECT_EQ(group1["ip"], formatted ? Value("2") : Value(2));
                    if (scope == Scope::site)
                    {
                        EXPECT_EQ(group1["c"], "hello");
                    }
                }

                if (scope == Scope::site)
                {
                    auto group2 = local1["g2"];
                    ASSERT_EQ(group2.size(), includeRules ? 3 : 2);
                    EXPECT_EQ(group2["i"], formatted ? Value("2") : Value(2));
                    EXPECT_EQ(group2["t"], "second of 0");
                    if (includeRules)
                    {
                        EXPECT_EQ(group2["im"], formatted ? Value("1") : Value(1));
                    }
                }
            }

            auto local2 = testResources["R2"];
            ASSERT_EQ(local2.size(), (scope == Scope::site) ? 3 : 2);
            {
                EXPECT_EQ(local2["_"]["name"], "TR2");

                auto group1 = local2["g1"];
                ASSERT_EQ(group1.size(), includeRules ? ((scope == Scope::site) ? 4 : 3) : 2);
                EXPECT_EQ(group1["i"], formatted ? Value("0.02 KB") : Value(21));
                EXPECT_EQ(group1["t"], "first of 2");
                if (includeRules)
                {
                    EXPECT_EQ(group1["ip"], formatted ? Value("22") : Value(22));
                    if (scope == Scope::site)
                    {
                        EXPECT_EQ(group1["c"], "hello");
                    }
                }

                if (scope == Scope::site)
                {
                    auto group2 = local2["g2"];
                    EXPECT_EQ(group2["i"], formatted ? Value("22") : Value(22));
                    EXPECT_EQ(group2["t"], "second of 2");
                    if (includeRules)
                    {
                        EXPECT_EQ(group2["im"], formatted ? Value("21") : Value(21));
                    }
                }
            }

            auto remote = testResources["R1"];
            if (scope == Scope::site)
            {
                ASSERT_EQ(remote.size(), 1);

                auto group2 = remote["g2"];
                ASSERT_EQ(group2.size(), includeRules ? 3 : 2);
                EXPECT_EQ(group2["i"], formatted ? Value("12") : Value(12));
                EXPECT_EQ(group2["t"], "second of 1");
            }
        }

        m_resources[0]->update("i1", 1024);
        m_resources[1]->update("t2", "hello");
        {
            auto systemValues = m_systemController.values(scope, formatted);
            EXPECT_EQ(systemValues.size(), 1);

            auto testResources = systemValues["tests"];
            ASSERT_EQ(testResources.size(), scope == Scope::site ? 3 : 2);

            auto local1 = testResources["R0"];
            EXPECT_EQ(local1["g1"]["i"], formatted ? Value("1 KB") : Value(1024));
            EXPECT_EQ(local1["g1"]["t"], "first of 0");
            if (includeRules)
            {
                EXPECT_EQ(local1["g1"]["ip"], formatted ? Value("1025") : Value(1025));
                if (scope == Scope::site)
                {
                    EXPECT_EQ(local1["g1"]["c"], "hello");
                }
            }
            if (scope == Scope::site)
            {
                EXPECT_EQ(local1["g2"]["i"], formatted ? Value("2") : Value(2));
                EXPECT_EQ(local1["g2"]["t"], "second of 0");
            }

            auto local2 = testResources["R2"];
            EXPECT_EQ(local2["g1"]["i"], formatted ? Value("0.02 KB") : Value(21));
            EXPECT_EQ(local2["g1"]["t"], "first of 2");
            if (includeRules)
            {
                EXPECT_EQ(local2["g1"]["ip"], formatted ? Value("22") : Value(22));
                if (scope == Scope::site)
                {
                    EXPECT_EQ(local2["g1"]["c"], "hello");
                }
            }
            if (scope == Scope::site)
            {
                EXPECT_EQ(local2["g2"]["i"], formatted ? Value("22") : Value(22));
                EXPECT_EQ(local2["g2"]["t"], "second of 2");
            }

            auto remote = testResources["R1"];
            if (scope == Scope::site)
            {
                ASSERT_EQ(remote.size(), 1);

                auto group2 = remote["g2"];
                ASSERT_EQ(group2.size(), includeRules ? 3 : 2);
                EXPECT_EQ(group2["i"], formatted ? Value("12") : Value(12));
                EXPECT_EQ(group2["t"], "hello");
                if (includeRules)
                {
                    EXPECT_EQ(group2["im"], formatted ? Value("11") : Value(11));
                }
            }
        }
    }

    void testValuesWithScope(Scope scope, bool includeRules, bool formatted, const nx::utils::DotNotationString& filter)
    {
        #define EXPECT_VALUE(RESOURCE, GROUP, PARAMETER, VALUE) \
        do { \
            if (filter.accepts("tests")) \
            { \
                auto testsIt = filter.findWithWildcard("tests"); \
                auto& testsFilter = (testsIt != filter.end() ? testsIt.value() : filter); \
                auto& tests = systemValues["tests"]; \
                if (testsFilter.accepts(RESOURCE)) \
                { \
                    auto resIt = testsFilter.findWithWildcard(RESOURCE); \
                    auto& resFilter = (resIt != testsFilter.end() ? resIt.value() : testsFilter); \
                    auto& res = tests[RESOURCE]; \
                    if (resFilter.accepts(GROUP)) \
                    { \
                        auto groupIt = resFilter.findWithWildcard(GROUP); \
                        auto& groupFilter = (groupIt != resFilter.end() ? groupIt.value() : resFilter); \
                        auto& group = res[GROUP]; \
                        if (groupFilter.accepts(PARAMETER)) \
                        { \
                            EXPECT_EQ(group[PARAMETER], VALUE); \
                        } \
                        else \
                        { \
                            ASSERT_EQ(group.find(PARAMETER), group.end()); \
                        } \
                    } \
                    else \
                    { \
                        ASSERT_EQ(res.find(GROUP), res.end()); \
                    } \
                } \
                else \
                { \
                    ASSERT_EQ(tests.find(RESOURCE), tests.end()); \
                } \
            } \
            else \
            { \
                ASSERT_EQ(systemValues.size(), 0); \
            } \
        } while(false)


        for (const auto& resource: m_resources)
            resource->setDefaults();

        auto [systemValues, actualScope] = m_systemController.valuesWithScope(scope, formatted, filter);

        EXPECT_VALUE("R0", "_", "name", "TR0");
        EXPECT_VALUE("R0", "g1", "i", formatted ? Value("0 KB") : Value(1));
        EXPECT_VALUE("R0", "g1", "t", "first of 0");
        if (includeRules)
        {
            EXPECT_VALUE("R0", "g1", "ip", formatted ? Value("2") : Value(2));
            if (scope == Scope::site)
                EXPECT_VALUE("R0", "g1", "c", "hello");
        }
        if (scope == Scope::site)
        {
            EXPECT_VALUE("R0", "g2", "i", formatted ? Value("2") : Value(2));
            EXPECT_VALUE("R0", "g2", "t", "second of 0");
            if (includeRules)
                EXPECT_VALUE("R0", "g2", "im", formatted ? Value("1") : Value(1));
        }

        EXPECT_VALUE("R2", "_", "name", "TR2");
        EXPECT_VALUE("R2", "g1", "i", formatted ? Value("0.02 KB") : Value(21));
        EXPECT_VALUE("R2", "g1", "t", "first of 2");
        if (includeRules)
        {
            EXPECT_VALUE("R2", "g1", "ip", formatted ? Value("22") : Value(22));
            if (scope == Scope::site)
                EXPECT_VALUE("R2", "g1", "c", "hello");
        }
        if (scope == Scope::site)
        {
            EXPECT_VALUE("R2", "g2", "i", formatted ? Value("22") : Value(22));
            EXPECT_VALUE("R2", "g2", "t", "second of 2");
            if (includeRules)
                EXPECT_VALUE("R2", "g2", "im", formatted ? Value("21") : Value(21));
        }

        if (scope == Scope::site)
        {
            EXPECT_VALUE("R1", "g2", "i", formatted ? Value("12") : Value(12));
            EXPECT_VALUE("R1", "g2", "t", "second of 1");
        }

        #undef EXPECT_VALUE
    }

    void testAlarms(Scope scope)
    {
        #define EXPECT_ALARM(RESOURCE, GROUP, PARAMETER, LEVEL, TEXT) \
        do { \
            EXPECT_EQ(alarms["tests"][RESOURCE][GROUP].size(), 1); \
            EXPECT_EQ(alarms["tests"][RESOURCE][GROUP][PARAMETER].size(), 1); \
            EXPECT_EQ(alarms["tests"][RESOURCE][GROUP][PARAMETER][0].level, api::metrics::AlarmLevel::LEVEL); \
            EXPECT_EQ(alarms["tests"][RESOURCE][GROUP][PARAMETER][0].text, TEXT); \
        } while(false)

        for (const auto& resource: m_resources)
            resource->setDefaults();
        {
            const auto alarms = m_systemController.alarms(scope);
            EXPECT_EQ(alarms.size(), 0);
        }

        m_resources[0]->update("i1", 150);
        m_resources[0]->update("i2", 50);
        m_resources[1]->update("i2", 70);
        m_resources[2]->update("i1", -2);
        {
            auto alarms = m_systemController.alarms(scope);
            EXPECT_EQ(alarms.size(), 1);
            EXPECT_EQ(alarms["tests"].size(), (scope == Scope::site) ? 3 : 2);

            EXPECT_ALARM("R0", "g1", "ip", warning, "i = 0.15 KB (>100), ip = 151");
            if (scope == Scope::site)
            {
                EXPECT_ALARM("R0", "g2", "i", warning, "i is 50 (>30)");
                EXPECT_ALARM("R1", "g2", "i", warning, "i is 70 (>30)");
                EXPECT_ALARM("R2", "g1", "ip", error, "i = -0 KB (<0), ip = -1");
            }
            else
            {
                EXPECT_ALARM("R2", "g1", "ip", error, "i = -0 KB (<0), ip = -1");
            }
        }

        for (const auto& resource: m_resources)
            resource->setDefaults();
        m_resources[2]->update("i2", 50);
        {
            auto alarms = m_systemController.alarms(scope);
            ASSERT_EQ(alarms.size(), (scope == Scope::site) ? 1 : 0);
            if (scope == Scope::site)
                EXPECT_ALARM("R2", "g2", "i", warning, "i is 50 (>30)");
        }

        #undef EXPECT_ALARM
    }

    void testAlarmsWithScope(Scope scope, const nx::utils::DotNotationString& filter)
    {
        #define EXPECT_ALARM(RESOURCE, GROUP, PARAMETER, LEVEL, TEXT) \
        do { \
            if (filter.accepts("tests")) \
            { \
                auto testsIt = filter.findWithWildcard("tests"); \
                auto& testsFilter = (testsIt != filter.end() ? testsIt.value() : filter); \
                auto& tests = alarms["tests"]; \
                if (testsFilter.accepts(RESOURCE)) \
                { \
                    auto resIt = testsFilter.findWithWildcard(RESOURCE); \
                    auto& resFilter = (resIt != testsFilter.end() ? resIt.value() : testsFilter); \
                    auto& res = tests[RESOURCE]; \
                    if (resFilter.accepts(GROUP)) \
                    { \
                        auto groupIt = resFilter.findWithWildcard(GROUP); \
                        auto& groupFilter = (groupIt != resFilter.end() ? groupIt.value() : resFilter); \
                        auto& group = res[GROUP]; \
                        if (groupFilter.accepts(PARAMETER)) \
                        { \
                            auto& para = group[PARAMETER]; \
                            EXPECT_EQ(para.size(), 1); \
                            EXPECT_EQ(para[0].level, api::metrics::AlarmLevel::LEVEL); \
                            EXPECT_EQ(para[0].text, TEXT); \
                        } \
                        else \
                        { \
                            ASSERT_EQ(group.find(PARAMETER), group.end()); \
                        } \
                    } \
                    else \
                    { \
                        ASSERT_EQ(res.find(GROUP), res.end()); \
                    } \
                } \
                else \
                { \
                    ASSERT_EQ(tests.find(RESOURCE), tests.end()); \
                } \
            } \
            else \
            { \
                ASSERT_EQ(alarms.size(), 0); \
            } \
        } while(false)

        for (const auto& resource: m_resources)
            resource->setDefaults();
        m_resources[0]->update("i1", 150);
        m_resources[0]->update("i2", 50);
        m_resources[1]->update("i2", 70);
        m_resources[2]->update("i1", -2);
        {
            auto [alarms, actualScope] = m_systemController.alarmsWithScope(scope, filter);

            EXPECT_ALARM("R0", "g1", "ip", warning, "i = 0.15 KB (>100), ip = 151");
            if (scope == Scope::site)
            {
                EXPECT_ALARM("R0", "g2", "i", warning, "i is 50 (>30)");
                EXPECT_ALARM("R1", "g2", "i", warning, "i is 70 (>30)");
                EXPECT_ALARM("R2", "g1", "ip", error, "i = -0 KB (<0), ip = -1");
            }
            else
            {
                EXPECT_ALARM("R2", "g1", "ip", error, "i = -0 KB (<0), ip = -1");
            }
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
    testValues(Scope::local, /*includeRules*/ false, /*formatted*/ false);
}

TEST_F(MetricsControllerTest, LocalValuesWithRules)
{
    setRules();
    testValues(Scope::local, /*includeRules*/ true, /*formatted*/ false);
}

TEST_F(MetricsControllerTest, SystemValues)
{
    testValues(Scope::site, /*includeRules*/ false, /*formatted*/ false);
}

TEST_F(MetricsControllerTest, SystemValuesWithRules)
{
    setRules();
    testValues(Scope::site, /*includeRules*/ true, /*formatted*/ false);
}

TEST_F(MetricsControllerTest, FormattedLocalValues)
{
    setRules();
    testValues(Scope::local, /*includeRules*/ true, /*formatted*/ true);
}

TEST_F(MetricsControllerTest, FormattedSystemValues)
{
    setRules();
    testValues(Scope::site, /*includeRules*/ true, /*formatted*/ true);
}

TEST_F(MetricsControllerTest, ValuesScenario)
{
    testValues(Scope::local, /*includeRules*/ false, /*formatted*/ false);
    testValues(Scope::site, /*includeRules*/ false, /*formatted*/ false);

    setRules();
    testValues(Scope::local, /*includeRules*/ true, /*formatted*/ false);
    testValues(Scope::site, /*includeRules*/ true, /*formatted*/ false);

    testValues(Scope::local, /*includeRules*/ true, /*formatted*/ true);
    testValues(Scope::site, /*includeRules*/ true, /*formatted*/ true);
}

TEST_F(MetricsControllerTest, ValuesWithScope)
{
    nx::utils::DotNotationString filter;
    testValuesWithScope(Scope::local, /*includeRules*/ false, /*formatted*/ false, filter);
    testValuesWithScope(Scope::site, /*includeRules*/ false, /*formatted*/ false, filter);

    filter.add("tests.R0._.name");
    testValuesWithScope(Scope::local, /*includeRules*/ false, /*formatted*/ false, filter);
    testValuesWithScope(Scope::site, /*includeRules*/ false, /*formatted*/ false, filter);

    filter.clear();
    filter.add("tests.R0.*.*");
    testValuesWithScope(Scope::local, /*includeRules*/ false, /*formatted*/ false, filter);
    testValuesWithScope(Scope::site, /*includeRules*/ false, /*formatted*/ false, filter);

    filter.clear();
    filter.add("tests.R0.*.i");
    filter.add("tests.R1.*.t");
    filter.add("tests.R2.*.name");
    testValuesWithScope(Scope::local, /*includeRules*/ false, /*formatted*/ false, filter);
    testValuesWithScope(Scope::site, /*includeRules*/ false, /*formatted*/ false, filter);

    filter.clear();
    filter.add("tests.*");
    testValuesWithScope(Scope::local, /*includeRules*/ false, /*formatted*/ false, filter);
    testValuesWithScope(Scope::site, /*includeRules*/ false, /*formatted*/ false, filter);

    filter.clear();
    filter.add("none");
    testValuesWithScope(Scope::local, /*includeRules*/ false, /*formatted*/ false, filter);
    testValuesWithScope(Scope::site, /*includeRules*/ false, /*formatted*/ false, filter);
}

TEST_F(MetricsControllerTest, LocalAlarms)
{
    ASSERT_EQ(m_systemController.alarms(Scope::local).size(), 0);
    setRules();
    testAlarms(Scope::local);
}

TEST_F(MetricsControllerTest, SystemAlarms)
{
    ASSERT_EQ(m_systemController.alarms(Scope::site).size(), 0);
    setRules();
    testAlarms(Scope::site);
}

TEST_F(MetricsControllerTest, AlarmsScenario)
{
    ASSERT_EQ(m_systemController.alarms(Scope::local).size(), 0);
    ASSERT_EQ(m_systemController.alarms(Scope::site).size(), 0);
    setRules();
    testAlarms(Scope::local);
    testAlarms(Scope::site);
}

TEST_F(MetricsControllerTest, AlarmsWithScope)
{
    nx::utils::DotNotationString filter;
    ASSERT_EQ(m_systemController.alarms(Scope::local).size(), 0);
    ASSERT_EQ(m_systemController.alarms(Scope::site).size(), 0);
    setRules();
    testAlarmsWithScope(Scope::local, filter);
    testAlarmsWithScope(Scope::site, filter);

    filter.clear();
    filter.add("none");
    testAlarmsWithScope(Scope::local, filter);
    testAlarmsWithScope(Scope::site, filter);

    filter.clear();
    filter.add("tests.*.g1.ip");
    testAlarmsWithScope(Scope::local, filter);
    testAlarmsWithScope(Scope::site, filter);

    filter.clear();
    filter.add("tests.R0.g2.i");
    testAlarmsWithScope(Scope::local, filter);
    testAlarmsWithScope(Scope::site, filter);
}

} // namespace nx::vms::utils::metrics::test
