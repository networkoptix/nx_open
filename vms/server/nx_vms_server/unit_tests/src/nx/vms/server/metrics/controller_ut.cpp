#include <gtest/gtest.h>

#include <nx/vms/server/metrics/controller.cpp>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/model_functions.h>

#include "test_providers.h"

void PrintTo(const QJsonValue& v, ::std::ostream* s)
{
    *s << QJson::serialized(v).toStdString();
}

namespace nx::vms::server::metrics::test {

static const QByteArray kRulesExample(R"json({
    "tests": {
        "number": { "alarms": { "warning": 2, "error": 3 } },
        "numberChanges": {
            "name": "number changes in last hour",
            "calculate": "count number",
            "insert": "after number",
            "alarms": { "warning": 1, "error": 10 }
        },
        "group": {
            "group": {
                "nameChanges": {
                    "name": "name changes in last hour",
                    "calculate": "count name",
                    "insert": "before name"
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
        controller.registerGroup(std::make_unique<TestResourceProvider>(1, 2, 3));
        controller.startMonitoring();
    }

    bool setRules()
    {
        api::metrics::SystemRules rules;
        if (!QJson::deserialize(kRulesExample, &rules))
            return false;

        controller.setRules(rules);
        return true;
    }

    static void checkParameter(
        const api::metrics::ParameterGroupManifest& manifest,
        const QString& id, const QString& name, size_t groupSize = 0)
    {
        EXPECT_EQ(id, manifest.id);
        EXPECT_EQ(name, manifest.name);
        EXPECT_EQ(groupSize, manifest.group.size());
    }

    template<typename Value>
    static void checkParameter(
        const std::map<QString /*id*/, api::metrics::ParameterGroupValues>& values,
        const QString& id, const Value& expectedValue, api::metrics::Status expectedStatus = {})
    {
        const auto item = values.find(id);
        ASSERT_TRUE(item != values.end()) << id.toStdString();

        EXPECT_EQ(api::metrics::Value(expectedValue), item->second.value) << id.toStdString();
        EXPECT_EQ(expectedStatus, item->second.status)
            << id.toStdString() << " = " << item->second.status.toStdString();
    }

protected:
    Controller controller;
};

TEST_F(MetricsControllerTest, Manifest)
{
    auto systemManifest = controller.manifest();
    EXPECT_EQ(1, systemManifest.size());

    auto testManifest = systemManifest["tests"];
    EXPECT_EQ(3, testManifest.size());

    checkParameter(testManifest[0], "name", "name parameter");
    checkParameter(testManifest[1], "number", "number parameter");

    checkParameter(testManifest[2], "group", "group in resource", 2);
    checkParameter(testManifest[2].group[0], "name", "name parameter");
    checkParameter(testManifest[2].group[1], "number", "number parameter");
}

TEST_F(MetricsControllerTest, Values)
{
    auto systemValues = controller.values();
    EXPECT_EQ(1, systemValues.size());

    auto testValues = systemValues["tests"];
    EXPECT_EQ(3, testValues.size());

    auto test1 = testValues[uuid(1).toSimpleString()];
    EXPECT_EQ("Test 1", test1.name);
    {
        EXPECT_EQ(3, test1.values.size());
        checkParameter(test1.values, "number", 1);
        checkParameter(test1.values, "name", "Test 1");

        auto group = test1.values["group"].group;
        checkParameter(group, "number", 1);
        checkParameter(group, "name", "Test 1");
    }

    auto test2 = testValues[uuid(2).toSimpleString()];
    EXPECT_EQ("Test 2", test2.name);
    {
        EXPECT_EQ(3, test2.values.size());
        checkParameter(test2.values, "number", 2);
        checkParameter(test2.values, "name", "Test 2");

        auto group = test2.values["group"].group;
        checkParameter(group, "number", 2);
        checkParameter(group, "name", "Test 2");
    }

    auto test3 = testValues[uuid(3).toSimpleString()];
    EXPECT_EQ("Test 3", test3.name);
    checkParameter(test3.values, "number", 3);

    EXPECT_TRUE(!test1.parent.isEmpty());
    EXPECT_EQ(test1.parent, test2.parent);
    EXPECT_EQ(test1.parent, test3.parent);
}

TEST_F(MetricsControllerTest, ManifestWithRules)
{
    ASSERT_TRUE(setRules());

    auto systemManifest = controller.manifest();
    EXPECT_EQ(1, systemManifest.size());

    auto testManifest = systemManifest["tests"];
    EXPECT_EQ(4, testManifest.size());

    checkParameter(testManifest[0], "name", "name parameter");
    checkParameter(testManifest[1], "number", "number parameter");
    checkParameter(testManifest[2], "numberChanges", "number changes in last hour");

    checkParameter(testManifest[3], "group", "group in resource", 3);
    checkParameter(testManifest[3].group[0], "nameChanges", "name changes in last hour");
    checkParameter(testManifest[3].group[1], "name", "name parameter");
    checkParameter(testManifest[3].group[2], "number", "number parameter");
}

TEST_F(MetricsControllerTest, ValuesWithRules)
{
    ASSERT_TRUE(setRules());

    auto systemValues = controller.values();
    EXPECT_EQ(1, systemValues.size());

    auto testValues = systemValues["tests"];
    EXPECT_EQ(3, testValues.size());

    auto test1 = testValues[uuid(1).toSimpleString()];
    EXPECT_EQ("Test 1", test1.name);
    {
        EXPECT_EQ(4, test1.values.size());
        checkParameter(test1.values, "number", 1);
        checkParameter(test1.values, "numberChanges", 0);
        checkParameter(test1.values, "name", "Test 1");

        auto group = test1.values["group"].group;
        checkParameter(group, "number", 1);
        checkParameter(group, "nameChanges", 0);
        checkParameter(group, "name", "Test 1");
    }

    auto test2 = testValues[uuid(2).toSimpleString()];
    EXPECT_EQ("Test 2", test2.name);
    {
        EXPECT_EQ(4, test2.values.size());
        checkParameter(test2.values, "number", 2, "warning");
        checkParameter(test1.values, "numberChanges", 0);
        checkParameter(test2.values, "name", "Test 2");

        auto group = test2.values["group"].group;
        checkParameter(group, "number", 2);
        checkParameter(group, "nameChanges", 0);
        checkParameter(group, "name", "Test 2");
    }

    auto test3 = testValues[uuid(3).toSimpleString()];
    EXPECT_EQ("Test 3", test3.name);
    checkParameter(test3.values, "number", 3, "error");

    EXPECT_TRUE(!test1.parent.isEmpty());
    EXPECT_EQ(test1.parent, test2.parent);
    EXPECT_EQ(test1.parent, test3.parent);
}

} // namespace nx::vms::server::metrics::test
