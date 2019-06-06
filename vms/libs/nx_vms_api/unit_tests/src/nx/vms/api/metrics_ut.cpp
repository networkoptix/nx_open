#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/vms/api/metrics.h>
#include <nx/utils/string.h>

namespace nx::vms::api::metrics::test {

template<typename T>
void expectSerrialization(const QByteArray& expectedJson, const T& value)
{
    const auto expectedValue = QJson::deserialized<T>(expectedJson);
    static const auto toJson =
        [](const auto& value) { return nx::utils::formatJsonString(QJson::serialized(value)); };

    EXPECT_EQ(toJson(expectedValue), toJson(value));
}

static const QByteArray kRulesExample(R"json({
    "servers": {
        "status": { "alarms": { "error": "Offline" } },
        "statusChanges": {
            "name": "status changes last hour",
            "calculate": "changes status 1h",
            "insert": "after status",
            "alarms": { "warning": 1, "error": 10 }
        },
        "cpuUsage": { "alarms": { "warning": 70, "error": 90 } },
        "cpuMaxUsage": {
            "name": "max CPU usage last hour",
            "calculate": "max cpuUsage 1h",
            "insert": "after cpuUsage",
            "alarms": { "warning": 70, "error": 90 }
        }
    },
    "cameras": {
        "status": { "alarms": { "warning": "Unauthorized", "error": "Offline" } },
        "primaryStream": {
            "name": "primary stream",
            "group": {
                "dropFps": {
                    "name": "FPS drop",
                    "calculate": "subtract targetFps actualFps",
                    "insert": "before targetFps",
                    "alarms": { "error": 2 }
                }
            }
        }
    }
})json");

TEST(Metrics, Rules)
{
    SystemRules rules;
    ASSERT_TRUE(QJson::deserialize(kRulesExample, &rules));
    EXPECT_EQ(2, rules.size());

    const auto checkRule =
        [&](const QString& group, const std::vector<QString> id,
            const std::map<Status, Value>& alarms = {},
            const QString& name = {}, const QString& calculate = {}, const QString& insert = {})
        {
            ParameterGroupRules* rule = &rules[group][id[0]];
            QString path = group + "." + id[0];
            for (auto i = 1; i < id.size(); ++i)
            {
                path += "." + id[i];
                rule = &rule->group[id[i]];
            }

            EXPECT_EQ(alarms, rule->alarms) << path.toStdString();
            EXPECT_EQ(name, rule->name) << path.toStdString();
            EXPECT_EQ(calculate, rule->calculate) << path.toStdString();
            EXPECT_EQ(insert, rule->insert) << path.toStdString();
        };

    checkRule("servers", {"status"}, {{"error", "Offline"}});
    checkRule("servers", {"statusChanges"}, {{"warning", 1}, {"error", 10}},
        "status changes last hour", "changes status 1h", "after status");

    checkRule("servers", {"cpuUsage"}, {{"warning", 70}, {"error", 90}});
    checkRule("servers", {"cpuMaxUsage"}, {{"warning", 70}, {"error", 90}},
        "max CPU usage last hour", "max cpuUsage 1h", "after cpuUsage");

    checkRule("cameras", {"status"}, {{"warning", "Unauthorized"}, {"error", "Offline"}});
    checkRule("cameras", {"primaryStream"}, {}, "primary stream");
    checkRule("cameras", {"primaryStream", "dropFps"}, {{"error", 2}},
        "FPS drop", "subtract targetFps actualFps", "before targetFps");

    expectSerrialization(kRulesExample, rules);
}

static const QByteArray kManifestExample(R"json({
    "servers": [
        { "id": "status", "name": "", "unit": "" },
        { "id": "cpuUsage", "name": "CPU usage", "unit": "%" }
    ],
    "cameras": [
        { "id": "status", "name": "", "unit": "" },
        {
            "id": "primaryStream",
            "name": "primary stream",
            "group": [
                { "id": "dropFps", "name": "FPS Drop", "unit": "FPS"},
                { "id": "actualFps", "name": "Actual FPS", "unit": "FPS"},
                { "id": "targetFps", "name": "Target FPS", "unit": "FPS"}
            ]
        }
    ]
})json");

TEST(Metrics, Manifest)
{
    static const SystemManifest manifest{
        {"servers", {
            makeParameterManifest("status"),
            makeParameterManifest("cpuUsage", "CPU usage", "%"),
        }},
        {"cameras", {
            makeParameterManifest("status"),
            makeParameterGroupManifest("primaryStream", "primary stream", {
                makeParameterManifest("dropFps", "FPS Drop", "FPS"),
                makeParameterManifest("actualFps", "Actual FPS", "FPS"),
                makeParameterManifest("targetFps", "Target FPS", "FPS"),
            }),
        }},
    };

    expectSerrialization(kManifestExample, manifest);
}

static const QByteArray kValuesExample(R"json(
{
    "servers": {
        "SERVER_1": {
            "name": "Server 1",
            "parent": "SYSTEM_1",
            "values": { "status": { "value": "Online" } }
        },
        "SERVER_2": {
            "name": "Server 2",
            "parent": "SYSTEM_1",
            "values": { "status": { "value": "Online" } }
        }
    },
    "cameras": {
        "CAMERA_1": {
            "name": "Camera 1",
            "parent": "SERVER_1",
            "values": {
                "status": { "value": "Online" },
                "primaryStream": {
                    "group": {
                        "actualFps": { "value": 27 },
                        "targetFps": { "value": 30 },
                        "dropFps": { "value": 3, "status": "warning" }
                    }
                }
            }
        },
        "CAMERA_2": {
            "name": "Camera 2",
            "parent": "SERVER_1",
            "values": { "status": { "status": "error", "value": "Offline" } }
        },
        "CAMERA_3": {
            "name": "Camera 3",
            "parent": "SERVER_2",
            "values": { "status": { "status": "warning", "value": "Unauthorized" } }
        }
    }
})json");

TEST(Metrics, Values)
{
    SystemValues serverOneValues;
    {
        auto& servers = serverOneValues["servers"];
        servers["SERVER_1"] = {"Server 1", "SYSTEM_1", {
            {"status", makeParameterValue("Online")},
        }};

        auto& cameras = serverOneValues["cameras"];
        cameras["CAMERA_1"] = {"Camera 1", "SERVER_1", {
            {"status", makeParameterValue("Online")},
            {"primaryStream", makeParameterGroupValue({
                {"actualFps", makeParameterValue(27)},
                {"targetFps", makeParameterValue(30)},
                {"dropFps", makeParameterValue(3, "warning")},
            })},
        }};

        cameras["CAMERA_2"] = {"Camera 2", "SERVER_1", {
            {"status", makeParameterValue("Offline", "error")},
        }};
    }

    SystemValues serverTwoValues;
    {
        auto& servers = serverOneValues["servers"];
        servers["SERVER_2"] = {"Server 2", "SYSTEM_1", {
            {"status", makeParameterValue("Online")},
        }};

        auto& cameras = serverOneValues["cameras"];
        cameras["CAMERA_3"] = {"Camera 3", "SERVER_2", {
            {"status", makeParameterValue("Unauthorized", "warning")},
        }};
    }

    expectSerrialization(kValuesExample, merge({serverOneValues, serverTwoValues}));
}

} // namespace nx::vms::api::metrics::test
