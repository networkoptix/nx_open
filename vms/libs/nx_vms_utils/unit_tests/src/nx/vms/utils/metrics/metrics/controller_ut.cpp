#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/vms/utils/metrics/controller.h>

#include "test_providers.h"

void PrintTo(const QJsonValue& v, ::std::ostream* s)
{
    *s << QJson::serialized(v).toStdString();
}

namespace nx::vms::utils::metrics::test {

static const QByteArray kRulesExample(R"json({
    "tests": {
        "i": { "alarms": { "warning": 7, "error": 8 } },
        "iChanges": {
            "name": "i changes in 1h",
            "calculate": "count i",
            "insert": "after i",
            "alarms": { "error": ">=1" }
        },
        "g": {
            "group": {
                "t": { "alarms": { "warning": "text_b1", "error": "=world" } },
                "tChanges": {
                    "name": "t changes in 1h",
                    "calculate": "count t",
                    "insert": "before t",
                    "alarms": { "warning": 2, "error": 10 }
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
        auto provider = std::make_unique<TestResourceProvider>();
        resourceProvider = provider.get();

        controller.registerGroup("tests", std::move(provider));
        controller.startMonitoring();

        for (int id = 0; id <= 2; ++id)
            resources.push_back(resourceProvider->makeResource(id, /*isLocal*/ id % 2 == 0));
    }

protected:
    bool setRules(bool isEnabled = true)
    {
        NX_INFO(this, "%1 rules", isEnabled ? "Set" : "Clear");

        api::metrics::SystemRules rules;
        if (isEnabled && !QJson::deserialize(kRulesExample, &rules))
            return false;

        controller.setRules(rules);
        return true;
    }

    void updateSomeValues()
    {
        NX_INFO(this, "Update values");

        resources[0]->update("i", 7);
        resources[0]->update("t", "hello");

        resources[1]->update("gi", 9);
        resources[1]->update("gt", "world");
    }

    static void checkParameter(
        const api::metrics::ParameterGroupManifest& manifest,
        const QString& id, const QString& name, size_t groupSize = 0)
    {
        EXPECT_EQ(id, manifest.id);
        EXPECT_EQ(name, manifest.name);
        EXPECT_EQ(groupSize, manifest.group.size());
    }

    void expectManifest(api::metrics::SystemManifest systemManifest)
    {
        NX_INFO(this, __func__);
        EXPECT_EQ(1, systemManifest.size());

        auto testManifest = systemManifest["tests"];
        EXPECT_EQ(3, testManifest.size());

        checkParameter(testManifest[0], "i", "int parameter");
        checkParameter(testManifest[1], "t", "text parameter");

        checkParameter(testManifest[2], "g", "group in resource", 2);
        checkParameter(testManifest[2].group[0], "i", "int parameter");
        checkParameter(testManifest[2].group[1], "t", "text parameter");
    }

    void expectManifestWithRules(api::metrics::SystemManifest systemManifest)
    {
        NX_INFO(this, __func__);
        EXPECT_EQ(1, systemManifest.size());

        auto testManifest = systemManifest["tests"];
        EXPECT_EQ(4, testManifest.size());

        checkParameter(testManifest[0], "i", "int parameter");
        checkParameter(testManifest[1], "iChanges", "i changes in 1h");
        checkParameter(testManifest[2], "t", "text parameter");

        checkParameter(testManifest[3], "g", "group in resource", 3);
        checkParameter(testManifest[3].group[0], "i", "int parameter");
        checkParameter(testManifest[3].group[1], "tChanges", "t changes in 1h");
        checkParameter(testManifest[3].group[2], "t", "text parameter");
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

    void expectCurrentValues(api::metrics::SystemValues systemValues, bool includeRemote)
    {
        NX_INFO(this, "%1 includeRemote=%2", __func__, includeRemote);
        EXPECT_EQ(1, systemValues.size());

        auto testValues = systemValues["tests"];
        EXPECT_EQ(includeRemote ? 3 : 2, testValues.size());

        auto test0 = testValues["test_0"];
        {
            EXPECT_EQ("Test 0", test0.name);

            EXPECT_EQ(3, test0.values.size());
            checkParameter(test0.values, "i", 0);
            checkParameter(test0.values, "t", "text_a0");

            auto group = test0.values["g"].group;
            EXPECT_EQ(2, group.size());
            checkParameter(group, "i", 10);
            checkParameter(group, "t", "text_b0");
        }

        auto test1 = testValues["test_1"];
        if (includeRemote)
        {
            EXPECT_EQ("Test 1", test1.name);

            EXPECT_EQ(3, test1.values.size());
            checkParameter(test1.values, "i", 1);
            checkParameter(test1.values, "t", "text_a1");

            auto group = test1.values["g"].group;
            EXPECT_EQ(2, group.size());
            checkParameter(group, "i", 11);
            checkParameter(group, "t", "text_b1");
        }
        else
        {
            EXPECT_EQ("", test1.name);
            EXPECT_EQ(0, test1.values.size());
        }

        auto test2 = testValues["test_2"];
        EXPECT_EQ("Test 2", test2.name);
    }

    void expectUpdatedValues(api::metrics::SystemValues systemValues, bool includeRemote)
    {
        NX_INFO(this, "%1 includeRemote=%2", __func__, includeRemote);
        EXPECT_EQ(1, systemValues.size());

        auto testValues = systemValues["tests"];
        EXPECT_EQ(includeRemote ? 3 : 2, testValues.size());

        auto test0 = testValues["test_0"];
        {
            EXPECT_EQ("Test 0", test0.name);

            EXPECT_EQ(3, test0.values.size());
            checkParameter(test0.values, "i", 7);
            checkParameter(test0.values, "t", "hello");

            auto group = test0.values["g"].group;
            EXPECT_EQ(2, group.size());
            checkParameter(group, "i", 10);
            checkParameter(group, "t", "text_b0");
        }

        auto test1 = testValues["test_1"];
        if (includeRemote)
        {
            EXPECT_EQ("Test 1", test1.name);

            EXPECT_EQ(3, test1.values.size());
            checkParameter(test1.values, "i", 1);
            checkParameter(test1.values, "t", "text_a1");

            auto group = test1.values["g"].group;
            EXPECT_EQ(2, group.size());
            checkParameter(group, "i", 9);
            checkParameter(group, "t", "world");
        }
        else
        {
            EXPECT_EQ("", test1.name);
            EXPECT_EQ(0, test1.values.size());
        }

        auto test2 = testValues["test_2"];
        EXPECT_EQ("Test 2", test2.name);
    }

    void expectUpdatedValuesWithRules(api::metrics::SystemValues systemValues, bool includeRemote)
    {
        NX_INFO(this, "%1 includeRemote=%2", __func__, includeRemote);
        EXPECT_EQ(1, systemValues.size());

        auto testValues = systemValues["tests"];
        EXPECT_EQ(includeRemote ? 3 : 2, testValues.size());

        auto test0 = testValues["test_0"];
        {
            EXPECT_EQ("Test 0", test0.name);

            EXPECT_EQ(4, test0.values.size());
            checkParameter(test0.values, "i", 7, "warning");
            checkParameter(test0.values, "iChanges", 1, "error");
            checkParameter(test0.values, "t", "hello");

            auto group = test0.values["g"].group;
            EXPECT_EQ(3, group.size());
            checkParameter(group, "i", 10);
            checkParameter(group, "tChanges", 0);
            checkParameter(group, "t", "text_b0");
        }

        auto test1 = testValues["test_1"];
        if (includeRemote)
        {
            EXPECT_EQ("Test 1", test1.name);

            EXPECT_EQ(4, test1.values.size());
            checkParameter(test1.values, "i", 1);
            checkParameter(test1.values, "iChanges", 0);
            checkParameter(test1.values, "t", "text_a1");

            auto group = test1.values["g"].group;
            EXPECT_EQ(3, group.size());
            checkParameter(group, "i", 9);
            checkParameter(group, "tChanges", 1);
            checkParameter(group, "t", "world", "error");
        }
        else
        {
            EXPECT_EQ("", test1.name);
            EXPECT_EQ(0, test1.values.size());
        }

        auto test2 = testValues["test_2"];
        EXPECT_EQ("Test 2", test2.name);
    }

    static void checkParameterTimeline(
        const std::map<QString /*id*/, api::metrics::ParameterGroupValues>& values,
        const QString& id, int expectedValueCount)
    {
        const auto item = values.find(id);
        ASSERT_TRUE(item != values.end()) << id.toStdString();

        const auto value = item->second.value;
        ASSERT_EQ(Value::Object, value.type()) << id.toStdString();
        ASSERT_EQ(expectedValueCount, value.toObject().size()) << id.toStdString();
    }

    void expectTimelineValues(
        api::metrics::SystemValues systemValues, 
        bool isUpdated, bool includeRemote)
    {
        NX_INFO(this, "%1 isUpdated=%2, includeRemote=%3", __func__, isUpdated, includeRemote);
        EXPECT_EQ(1, systemValues.size());

        auto testValues = systemValues["tests"];
        EXPECT_EQ(includeRemote ? 3 : 2, testValues.size());

        auto test0 = testValues["test_0"];
        {
            EXPECT_EQ("Test 0", test0.name);

            EXPECT_EQ(3, test0.values.size());
            checkParameterTimeline(test0.values, "i", isUpdated ? 2 : 1);
            checkParameterTimeline(test0.values, "t", isUpdated ? 2 : 1);

            auto group = test0.values["g"].group;
            EXPECT_EQ(2, group.size());
            checkParameterTimeline(group, "i", 1);
            checkParameterTimeline(group, "t", 1);
        }

        auto test1 = testValues["test_1"];
        if (includeRemote)
        {
            EXPECT_EQ("Test 1", test1.name);

            EXPECT_EQ(3, test1.values.size());
            checkParameterTimeline(test1.values, "i", 1);
            checkParameterTimeline(test1.values, "t", 1);

            auto group = test1.values["g"].group;
            EXPECT_EQ(2, group.size());
            checkParameterTimeline(group, "i", isUpdated ? 2 : 1);
            checkParameterTimeline(group, "t", isUpdated ? 2 : 1);
        }
        else
        {
            EXPECT_EQ("", test1.name);
            EXPECT_EQ(0, test1.values.size());
        }
    }

protected:
    Controller controller;
    TestResourceProvider* resourceProvider = nullptr;
    std::vector<std::shared_ptr<TestResource>> resources;
};

TEST_F(MetricsControllerTest, Manifest)
{
    expectManifest(controller.manifest(Controller::none));

    ASSERT_TRUE(setRules());

    expectManifestWithRules(controller.manifest(Controller::applyRules));
    expectManifest(controller.manifest(Controller::none));
}

TEST_F(MetricsControllerTest, Values)
{
    expectCurrentValues(
        controller.values(Controller::none), 
        /*includeRemote*/ false);
    expectTimelineValues(
        controller.values(Controller::none, std::chrono::milliseconds::zero()),
        /*isUpdated*/ false, /*includeRemote*/ false);

    expectCurrentValues(
        controller.values(Controller::includeRemote), 
        /*includeRemote*/ true);
    expectTimelineValues(
        controller.values(Controller::includeRemote, std::chrono::milliseconds::zero()),
        /*isUpdated*/ false, /*includeRemote*/ true);

    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);
    timeShift.applyRelativeShift(std::chrono::minutes(10));
    updateSomeValues();

    expectUpdatedValues(
        controller.values(Controller::none), 
        /*includeRemote*/ false);
    expectTimelineValues(
        controller.values(Controller::none, std::chrono::milliseconds::zero()),
        /*isUpdated*/ true, /*includeRemote*/ false);

    expectUpdatedValues(
        controller.values(Controller::includeRemote), 
        /*includeRemote*/ true);
    expectTimelineValues(
        controller.values(Controller::includeRemote, std::chrono::milliseconds::zero()),
        /*isUpdated*/ true, /*includeRemote*/ true);

    ASSERT_TRUE(setRules());

    expectUpdatedValuesWithRules(
        controller.values(Controller::applyRules), 
        /*includeRemote*/ false);
    expectUpdatedValues(
        controller.values(Controller::none), 
        /*includeRemote*/ false);

    expectUpdatedValuesWithRules(
        controller.values({Controller::includeRemote, Controller::applyRules}),
        /*includeRemote*/ true);
    expectUpdatedValues(
        controller.values(Controller::includeRemote), 
        /*includeRemote*/ true);
}

} // namespace nx::vms::utils::metrics::test
