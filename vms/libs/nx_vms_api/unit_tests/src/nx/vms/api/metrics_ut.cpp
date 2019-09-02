#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/vms/api/metrics.h>
#include <nx/utils/string.h>

namespace nx::vms::api::metrics::test {

template<typename T>
void expectSerialization(const QByteArray& expectedJson, const T& value)
{
    const auto expectedValue = QJson::deserialized<T>(expectedJson);
    static const auto toJson =
        [](const auto& value) { return nx::utils::formatJsonString(QJson::serialized(value)); };

    EXPECT_EQ(toJson(expectedValue), toJson(value));
}

static const QByteArray kRulesExample(R"json({
    "servers": {
        "availability": {
            "status": { "alarms": [{ "level": "danger", "condition": "ne status 'Online'" }]},
            "offlineEvents": { "alarms": [{ "level": "warning", "condition": "gt offlineEvents 0" }]}
        },
        "load": {
            "recommendedCpuUsageP": { "calculate": "const 95" },
            "totalCpuUsageP": { "alarms": [{
                "level": "warning",
                "condition": "gt totalCpuUsageP recommendedCpuUsageP"
            }]}
        }
    },
    "cameras": {
        "info": {
            "status": { "alarms": [
                { "level": "warning", "condition": "eq status 'Unauthorized'" },
                { "level": "danger", "condition": "eq status 'Offline'" }
            ]}
        },
        "issues": { "group": {
            "offlineEvents": { "alarms": [{ "level": "warning", "condition": "gt offlineEvents 0" }] },
            "streamIssues": { "alarms": [{ "level": "warning", "condition": "gt streamIssues 0" }] }
        }}
    }
})json");

TEST(Metrics, Rules)
{
    SystemRules rules;
    ASSERT_TRUE(QJson::deserialize(kRulesExample, &rules));
    EXPECT_EQ(2, rules.size());

    auto servers = rules["servers"];
    {
        auto status = servers["availability"]["status"];
        ASSERT_EQ(status.alarms.size(), 1);
        EXPECT_EQ(status.alarms[0].level, "danger");
        EXPECT_EQ(status.alarms[0].condition, "ne status 'Online'");

        auto events = servers["availability"]["offlineEvents"];
        ASSERT_EQ(events.alarms.size(), 1);
        EXPECT_EQ(events.alarms[0].level, "warning");
        EXPECT_EQ(events.alarms[0].condition, "gt offlineEvents 0");
    }

    auto cameras = rules["cameras"];
    {
        auto status = cameras["info"]["status"];
        ASSERT_EQ(status.alarms.size(), 2);
        EXPECT_EQ(status.alarms[0].level, "warning");
        EXPECT_EQ(status.alarms[0].condition, "eq status 'Unauthorized'");
        EXPECT_EQ(status.alarms[1].level, "danger");
        EXPECT_EQ(status.alarms[1].condition, "eq status 'Offline'");
    }

    expectSerialization(kRulesExample, rules);
}

static const QByteArray kManifestExample(R"json({
  "servers": [
    {
        "id": "availability",
        "name": "Availability",
        "values": [
            { "id": "status", "name": "Status", "display": "table&panel" },
            { "id": "offlineEvents", "name": "Offline Events", "display": "table&panel" }
        ]
    }, {
        "id": "load",
        "name": "Load",
        "values": [
            { "id": "totalCpuUsageP", "name": "CPU Usage", "unit": "%", "display": "table&panel" },
            { "id": "serverCpuUsageP", "name": "CPU Usage (VMS Server)", "unit": "%", "display": "table&panel" }
        ]
    }
  ],
  "cameras": [
    {
        "id": "info",
        "name": "Info",
        "values": [
            { "id": "type", "name": "Type", "display": "table&panel" },
            { "id": "ip", "name": "IP", "display": "table&panel" }
        ]
    }, {
        "id": "issues",
        "name": "Issues",
        "values": [
            { "id": "offlineEvents", "name": "Offline Events", "display": "table&panel" },
            { "id": "streamIssues", "name": "Stream Issues (1h)", "display": "table&panel" }
        ]
    }
  ]
})json");

TEST(Metrics, Manifest)
{
    SystemManifest manifest;
    ASSERT_TRUE(QJson::deserialize(kManifestExample, &manifest));
    EXPECT_EQ(2, manifest.size());

    auto servers = manifest["servers"];
    ASSERT_EQ(servers.size(), 2);
    {
        const auto availability = servers[0];
        EXPECT_EQ(availability.id, "availability");
        EXPECT_EQ(availability.name, "Availability");
        ASSERT_EQ(availability.values.size(), 2);

        const auto status = availability.values[0];
        EXPECT_EQ(status.id, "status");
        EXPECT_EQ(status.name, "Status");
        EXPECT_EQ(status.display, "table&panel");

        const auto events = availability.values[1];
        EXPECT_EQ(events.id, "offlineEvents");
        EXPECT_EQ(events.name, "Offline Events");
        EXPECT_EQ(events.display, "table&panel");

        const auto load = servers[1];
        EXPECT_EQ(load.id, "load");
        EXPECT_EQ(load.name, "Load");
        ASSERT_EQ(load.values.size(), 2);

        const auto cpuUsage = load.values[0];
        EXPECT_EQ(cpuUsage.id, "totalCpuUsageP");
        EXPECT_EQ(cpuUsage.name, "CPU Usage");
        EXPECT_EQ(cpuUsage.unit, "%");
        EXPECT_EQ(cpuUsage.display, "table&panel");
    }

    auto cameras = manifest["cameras"];
    ASSERT_EQ(servers.size(), 2);
    {
        const auto info = cameras[0];
        EXPECT_EQ(info.id, "info");
        EXPECT_EQ(info.name, "Info");
        ASSERT_EQ(info.values.size(), 2);

        const auto type = info.values[0];
        EXPECT_EQ(type.id, "type");
        EXPECT_EQ(type.name, "Type");
        EXPECT_EQ(type.display, "table&panel");
    }

    expectSerialization(kManifestExample, manifest);
}

static const QByteArray kValuesExample(R"json({
    "servers": {
        "SERVER_UUID_1": {
            "name": "Server 1",
            "parent": "SYSTEM_UUID_1",
            "values": {
                "availability": { "status": "Online", "offlineEvents": 0 },
                "load": { "totalCpuUsageP": 95 }
            }
        },
        "SERVER_UUID_2": {
            "name": "Server 2",
            "parent": "SYSTEM_UUID_1",
            "values": {
                "availability": { "status": "Offline" }
            }
        }
    },
    "cameras": {
        "CAMERA_UUID_1": {
            "name": "DWC-112233",
            "parent": "SERVER_UUID_1",
            "values": {
                "info": { "type": "camera", "ip": "192.168.0.101", "status": "Online" },
                "issues": { "offlineEvents": 1, "streamIssues": 2, "ipConflicts": 3 }
            }
        },
        "CAMERA_UUID_2": {
            "name": "iqEve-666",
            "parent": "SERVER_UUID_2",
            "values": {
                "info": { "type": "camera", "ip": "192.168.0.103", "status": "Offline" },
                "issues": { "offlineEvents": 5, "streamIssues": 0, "ipConflicts": 0 }
            }
        }
    }
})json");

TEST(Metrics, Values)
{
    SystemValues values;
    ASSERT_TRUE(QJson::deserialize(kValuesExample, &values));
    EXPECT_EQ(2, values.size());

    auto servers = values["servers"];
    EXPECT_EQ(servers.size(), 2);
    {
        auto s1 = servers["SERVER_UUID_1"];
        EXPECT_EQ(s1.name, "Server 1");
        EXPECT_EQ(s1.parent, "SYSTEM_UUID_1");
        EXPECT_EQ(s1.values.size(), 2);

        EXPECT_EQ(s1.values["availability"]["status"], "Online");
        EXPECT_EQ(s1.values["availability"]["offlineEvents"], 0);
        EXPECT_EQ(s1.values["load"]["totalCpuUsageP"], 95);

        auto s2 = servers["SERVER_UUID_2"];
        EXPECT_EQ(s2.name, "Server 2");
        EXPECT_EQ(s2.parent, "SYSTEM_UUID_1");
        EXPECT_EQ(s2.values.size(), 1);

        EXPECT_EQ(s2.values["availability"]["status"], "Offline");
    }

    auto cameras = values["cameras"];
    EXPECT_EQ(cameras.size(), 2);
    {
        auto c1 = cameras["CAMERA_UUID_1"];
        EXPECT_EQ(c1.name, "DWC-112233");
        EXPECT_EQ(c1.parent, "SERVER_UUID_1");
        EXPECT_EQ(c1.values.size(), 2);

        EXPECT_EQ(c1.values["info"]["type"], "camera");
        EXPECT_EQ(c1.values["info"]["ip"], "192.168.0.101");
        EXPECT_EQ(c1.values["info"]["status"], "Online");

        EXPECT_EQ(c1.values["issues"]["offlineEvents"], 1);
        EXPECT_EQ(c1.values["issues"]["streamIssues"], 2);
        EXPECT_EQ(c1.values["issues"]["ipConflicts"], 3);

        auto c2 = cameras["CAMERA_UUID_2"];
        EXPECT_EQ(c2.name, "iqEve-666");
        EXPECT_EQ(c2.parent, "SERVER_UUID_2");
        EXPECT_EQ(c2.values.size(), 2);

        EXPECT_EQ(c2.values["info"]["type"], "camera");
        EXPECT_EQ(c2.values["info"]["ip"], "192.168.0.103");
        EXPECT_EQ(c2.values["info"]["status"], "Offline");
    }

    SystemValues serverOneValues;
    serverOneValues["servers"]["SERVER_UUID_1"] = values["servers"]["SERVER_UUID_1"];
    serverOneValues["cameras"]["CAMERA_UUID_1"] = values["cameras"]["CAMERA_UUID_1"];

    SystemValues serverTwoValues;
    serverOneValues["servers"]["SERVER_UUID_2"] = values["servers"]["SERVER_UUID_2"];
    serverOneValues["cameras"]["CAMERA_UUID_2"] = values["cameras"]["CAMERA_UUID_2"];

    SystemValues mergedValues;
    merge(&mergedValues, &serverOneValues);
    merge(&mergedValues, &serverTwoValues);
    expectSerialization(kValuesExample, mergedValues);
}

static const QByteArray kAlarmsExample(R"json([
  {
    "resource": "SERVER_UUID_1",
    "parameter": "load.totalCpuUsageP",
    "level": "danger",
    "text": "CPU Usage is 95%"
  }, {
    "resource": "SERVER_UUID_2",
    "parameter": "availability.status",
    "level": "danger",
    "text": "Status is Offline"
  }, {
    "resource": "CAMERA_UUID_1",
    "parameter": "primaryStream.targetFps",
    "level": "warning",
    "text": "Actual FPS is 30"
  }, {
    "resource": "CAMERA_UUID_3",
    "parameter": "issues.offlineEvents",
    "level": "warning",
    "text": "Offline Events is 2"
  }
])json");

TEST(Metrics, Alarms)
{
    std::vector<Alarm> alarms;
    ASSERT_TRUE(QJson::deserialize(kAlarmsExample, &alarms));
    ASSERT_EQ(alarms.size(), 4);

    const auto s1 = alarms[0];
    EXPECT_EQ(s1.resource, "SERVER_UUID_1");
    EXPECT_EQ(s1.parameter, "load.totalCpuUsageP");
    EXPECT_EQ(s1.level, "danger");
    EXPECT_EQ(s1.text, "CPU Usage is 95%");

    const auto s2 = alarms[1];
    EXPECT_EQ(s2.resource, "SERVER_UUID_2");
    EXPECT_EQ(s2.parameter, "availability.status");
    EXPECT_EQ(s2.level, "danger");
    EXPECT_EQ(s2.text, "Status is Offline");

    const auto c1 = alarms[2];
    EXPECT_EQ(c1.resource, "CAMERA_UUID_1");
    EXPECT_EQ(c1.parameter, "primaryStream.targetFps");
    EXPECT_EQ(c1.level, "warning");
    EXPECT_EQ(c1.text, "Actual FPS is 30");

    const auto c2 = alarms[3];
    EXPECT_EQ(c2.resource, "CAMERA_UUID_3");
    EXPECT_EQ(c2.parameter, "issues.offlineEvents");
    EXPECT_EQ(c2.level, "warning");
    EXPECT_EQ(c2.text, "Offline Events is 2");
}

} // namespace nx::vms::api::metrics::test
