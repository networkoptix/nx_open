// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/string.h>
#include <nx/vms/api/metrics.h>

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
        "name": "Servers",
        "resource": "Server",
        "values": {
            "availability": {
                "name": "Availability Info",
                "values": {
                    "status": {
                        "display": "table|panel",
                        "alarms": [{ "level": "error", "condition": "ne status 'Online'" }]
                    },
                    "offlineEvents": {
                        "name": "Offline events",
                        "display": "table",
                        "description": "Shows how many times the server went offline",
                        "alarms": [{ "level": "warning", "condition": "gt offlineEvents 0" }]
                    }
                }
            },
            "load": {
                "values": {
                    "recommendedCpuUsageP": { "calculate": "const 95" },
                    "totalCpuUsageP": {
                        "name": "CPU Usage",
                        "description": "Total CPU Usage",
                        "display": "panel",
                        "format": "%",
                        "alarms": [ {
                            "level": "warning",
                            "condition": "gt totalCpuUsageP recommendedCpuUsageP"
                        } ]
                    }
                }
            }
        }
    },
    "cameras": {
        "name": "Cameras",
        "resource": "Camera",
        "values": {
            "info": {
                "values": {
                    "status": { "alarms": [
                        { "level": "warning", "condition": "eq status 'Unauthorized'" },
                        { "level": "error", "condition": "eq status 'Offline'" }
                    ]}
                }
            },
            "issues": {
                "values": {
                    "offlineEvents": { "alarms": [{ "level": "warning", "condition": "gt offlineEvents 0" }] },
                    "streamIssues": { "alarms": [{ "level": "warning", "condition": "gt streamIssues 0" }] }
                }
            }
        }
    }
})json");

TEST(Metrics, Rules)
{
    SiteRules rules;
    ASSERT_TRUE(QJson::deserialize(kRulesExample, &rules));
    EXPECT_EQ(2, rules.size());

    auto servers = rules["servers"];
    {
        EXPECT_EQ(servers.name, "Servers");
        EXPECT_EQ(servers.resource, "Server");
        EXPECT_EQ(servers.values["availability"].name, "Availability Info");

        auto status = servers.values["availability"].values["status"];
        EXPECT_EQ(status.name, "");
        EXPECT_EQ(status.description, "");
        EXPECT_EQ(status.display, Displays(Display::both));
        EXPECT_EQ(status.format, "");
        ASSERT_EQ(status.alarms.size(), 1);
        EXPECT_EQ(status.alarms[0].level, AlarmLevel::error);
        EXPECT_EQ(status.alarms[0].condition, "ne status 'Online'");

        auto events = servers.values["availability"].values["offlineEvents"];
        EXPECT_EQ(events.name, "Offline events");
        EXPECT_EQ(events.description, "Shows how many times the server went offline");
        EXPECT_EQ(events.display, Displays(Display::table));
        EXPECT_EQ(events.format, "");
        ASSERT_EQ(events.alarms.size(), 1);
        EXPECT_EQ(events.alarms[0].level, AlarmLevel::warning);
        EXPECT_EQ(events.alarms[0].condition, "gt offlineEvents 0");

        auto cpuUsage = servers.values["load"].values["totalCpuUsageP"];
        EXPECT_EQ(cpuUsage.name, "CPU Usage");
        EXPECT_EQ(cpuUsage.description, "Total CPU Usage");
        EXPECT_EQ(cpuUsage.display, Displays(Display::panel));
        EXPECT_EQ(cpuUsage.format, "%");
        ASSERT_EQ(cpuUsage.alarms.size(), 1);
        EXPECT_EQ(cpuUsage.alarms[0].level, AlarmLevel::warning);
        EXPECT_EQ(cpuUsage.alarms[0].condition, "gt totalCpuUsageP recommendedCpuUsageP");
    }

    auto cameras = rules["cameras"];
    {
        EXPECT_EQ(cameras.name, "Cameras");
        EXPECT_EQ(cameras.resource, "Camera");

        auto status = cameras.values["info"].values["status"];
        ASSERT_EQ(status.alarms.size(), 2);
        EXPECT_EQ(status.alarms[0].level, AlarmLevel::warning);
        EXPECT_EQ(status.alarms[0].condition, "eq status 'Unauthorized'");
        EXPECT_EQ(status.alarms[1].level, AlarmLevel::error);
        EXPECT_EQ(status.alarms[1].condition, "eq status 'Offline'");
    }

    expectSerialization(kRulesExample, rules);
}

static const QByteArray kManifestExample(R"json([
    {
        "id": "servers",
        "name": "Servers",
        "resource": "Server",
        "values": [
            {
                "id": "availability",
                "name": "Availability",
                "values": [
                    { "id": "name", "name": "Name", "display": "table|panel" },
                    { "id": "status", "name": "Status", "display": "table|panel" },
                    { "id": "offlineEvents", "name": "Offline Events", "display": "table|panel" }
                ]
            }, {
                "id": "load",
                "name": "Load",
                "values": [
                    { "id": "totalCpuUsageP", "name": "CPU Usage", "format": "%", "display": "table" },
                    { "id": "serverCpuUsageP", "name": "CPU Usage (VMS Server)", "format": "%", "display": "panel" }
                ]
            }
        ]
    }, {
        "id": "cameras",
        "name": "Cameras",
        "resource": "Camera",
        "values": [
            {
                "id": "info",
                "name": "Info",
                "values": [
                    { "id": "name", "name": "Name", "display": "table|panel" },
                    { "id": "server", "name": "Server", "display": "table|panel" },
                    { "id": "type", "name": "Type", "display": "" },
                    { "id": "ip", "name": "IP", "description": "Primary IP", "display": "table|panel" }
                ]
            }, {
                "id": "issues",
                "name": "Issues",
                "values": [
                    { "id": "offlineEvents", "name": "Offline Events", "display": "table|panel" },
                    { "id": "streamIssues", "name": "Stream Issues (1h)", "display": "table|panel" }
                ]
            }
        ]
    }
])json");

TEST(Metrics, Manifest)
{
    SiteManifest manifest;
    ASSERT_TRUE(QJson::deserialize(kManifestExample, &manifest));
    EXPECT_EQ(2, manifest.size());

    auto servers = manifest[0];
    EXPECT_EQ(servers.id, "servers");
    EXPECT_EQ(servers.name, "Servers");
    EXPECT_EQ(servers.resource, "Server");
    ASSERT_EQ(servers.values.size(), 2);
    {
        const auto availability = servers.values[0];
        EXPECT_EQ(availability.id, "availability");
        EXPECT_EQ(availability.name, "Availability");
        ASSERT_EQ(availability.values.size(), 3);

        const auto name = availability.values[0];
        EXPECT_EQ(name.id, "name");
        EXPECT_EQ(name.name, "Name");
        EXPECT_EQ(name.display, Displays(Display::both));

        const auto status = availability.values[1];
        EXPECT_EQ(status.id, "status");
        EXPECT_EQ(status.name, "Status");
        EXPECT_EQ(status.display, Displays(Display::both));

        const auto events = availability.values[2];
        EXPECT_EQ(events.id, "offlineEvents");
        EXPECT_EQ(events.name, "Offline Events");
        EXPECT_EQ(events.display, Displays(Display::both));

        const auto load = servers.values[1];
        EXPECT_EQ(load.id, "load");
        EXPECT_EQ(load.name, "Load");
        ASSERT_EQ(load.values.size(), 2);

        const auto cpuUsage = load.values[0];
        EXPECT_EQ(cpuUsage.id, "totalCpuUsageP");
        EXPECT_EQ(cpuUsage.name, "CPU Usage");
        EXPECT_EQ(cpuUsage.format, "%");
        EXPECT_EQ(cpuUsage.display, Displays(Display::table));

        const auto serverCpuUsage = load.values[1];
        EXPECT_EQ(serverCpuUsage.id, "serverCpuUsageP");
        EXPECT_EQ(serverCpuUsage.name, "CPU Usage (VMS Server)");
        EXPECT_EQ(serverCpuUsage.format, "%");
        EXPECT_EQ(serverCpuUsage.display, Displays(Display::panel));
    }

    auto cameras = manifest[1];
    EXPECT_EQ(cameras.id, "cameras");
    EXPECT_EQ(cameras.name, "Cameras");
    EXPECT_EQ(cameras.resource, "Camera");
    ASSERT_EQ(cameras.values.size(), 2);
    {
        const auto info = cameras.values[0];
        EXPECT_EQ(info.id, "info");
        EXPECT_EQ(info.name, "Info");
        ASSERT_EQ(info.values.size(), 4);

        const auto server = info.values[1];
        EXPECT_EQ(server.id, "server");
        EXPECT_EQ(server.name, "Server");
        EXPECT_EQ(server.display, Displays(Display::both));

        const auto type = info.values[2];
        EXPECT_EQ(type.id, "type");
        EXPECT_EQ(type.name, "Type");
        EXPECT_EQ(type.display, Displays(Display::none));

        const auto ip = info.values[3];
        EXPECT_EQ(ip.id, "ip");
        EXPECT_EQ(ip.name, "IP");
        EXPECT_EQ(ip.description, "Primary IP");
        EXPECT_EQ(ip.display, Displays(Display::both));
    }

    expectSerialization(kManifestExample, manifest);
}

static const QByteArray kValuesExample(R"json({
    "servers": {
        "SERVER_UUID_1": {
            "availability": { "name": "Server 1", "status": "Online", "offlineEvents": 0 },
            "load": { "totalCpuUsageP": 95 }
        },
        "SERVER_UUID_2": {
            "availability": {"name": "Server 2", "status": "Offline" }
        }
    },
    "cameras": {
        "CAMERA_UUID_1": {
            "info": {
                "name": "DWC-112233", "server": "Server 1",
                "type": "camera", "ip": "192.168.0.101", "status": "Online"
            },
            "issues": { "offlineEvents": 1, "streamIssues": 2, "ipConflicts": 3 }
        },
        "CAMERA_UUID_2": {
            "info": {
                "name": "iqEve-666", "server": "Server 2",
                "type": "camera", "ip": "192.168.0.103", "status": "Offline"
            },
            "issues": { "offlineEvents": 5, "streamIssues": 0, "ipConflicts": 0 }
        }
    }
})json");

TEST(Metrics, Values)
{
    SiteValues values;
    ASSERT_TRUE(QJson::deserialize(kValuesExample, &values));
    EXPECT_EQ(2, values.size());

    auto servers = values["servers"];
    EXPECT_EQ(servers.size(), 2);
    {
        auto s1 = servers["SERVER_UUID_1"];
        EXPECT_EQ(s1.size(), 2);
        EXPECT_EQ(s1["availability"].size(), 3);
        EXPECT_EQ(s1["load"].size(), 1);

        EXPECT_EQ(s1["availability"]["name"], "Server 1");
        EXPECT_EQ(s1["availability"]["status"], "Online");
        EXPECT_EQ(s1["availability"]["offlineEvents"], 0);
        EXPECT_EQ(s1["load"]["totalCpuUsageP"], 95);

        auto s2 = servers["SERVER_UUID_2"];
        EXPECT_EQ(s2.size(), 1);
        EXPECT_EQ(s2["availability"].size(), 2);

        EXPECT_EQ(s2["availability"]["name"], "Server 2");
        EXPECT_EQ(s2["availability"]["status"], "Offline");
    }

    auto cameras = values["cameras"];
    EXPECT_EQ(cameras.size(), 2);
    {
        auto c1 = cameras["CAMERA_UUID_1"];
        EXPECT_EQ(c1.size(), 2);
        EXPECT_EQ(c1["info"].size(), 5);
        EXPECT_EQ(c1["issues"].size(), 3);

        EXPECT_EQ(c1["info"]["name"], "DWC-112233");
        EXPECT_EQ(c1["info"]["server"], "Server 1");
        EXPECT_EQ(c1["info"]["type"], "camera");
        EXPECT_EQ(c1["info"]["ip"], "192.168.0.101");
        EXPECT_EQ(c1["info"]["status"], "Online");

        EXPECT_EQ(c1["issues"]["offlineEvents"], 1);
        EXPECT_EQ(c1["issues"]["streamIssues"], 2);
        EXPECT_EQ(c1["issues"]["ipConflicts"], 3);

        auto c2 = cameras["CAMERA_UUID_2"];
        EXPECT_EQ(c2.size(), 2);

        EXPECT_EQ(c2["info"]["name"], "iqEve-666");
        EXPECT_EQ(c2["info"]["server"], "Server 2");
        EXPECT_EQ(c2["info"]["type"], "camera");
        EXPECT_EQ(c2["info"]["ip"], "192.168.0.103");
        EXPECT_EQ(c2["info"]["status"], "Offline");
    }

    SiteValues serverOneValues;
    serverOneValues["servers"]["SERVER_UUID_1"] = values["servers"]["SERVER_UUID_1"];
    serverOneValues["cameras"]["CAMERA_UUID_1"] = values["cameras"]["CAMERA_UUID_1"];

    SiteValues serverTwoValues;
    serverOneValues["servers"]["SERVER_UUID_2"] = values["servers"]["SERVER_UUID_2"];
    serverOneValues["cameras"]["CAMERA_UUID_2"] = values["cameras"]["CAMERA_UUID_2"];

    SiteValues mergedValues;
    merge(&mergedValues, &serverOneValues);
    merge(&mergedValues, &serverTwoValues);
    expectSerialization(kValuesExample, mergedValues);
}

TEST(Metrics, ValuesMerge)
{
    SiteValues firstValues;
    firstValues["servers"]["SERVER_1"]["info"]["status"] = "Online";
    firstValues["servers"]["SERVER_1"]["info"]["publicIp"] = "192.168.0.1";
    firstValues["servers"]["SERVER_1"]["load"]["cpuUsageP"] = 50;
    firstValues["servers"]["SERVER_2"]["info"]["status"] = "Online";

    SiteValues secondValues;
    firstValues["servers"]["SERVER_2"]["info"]["publicIp"] = "192.168.0.2";
    firstValues["servers"]["SERVER_2"]["load"]["cpuUsageP"] = 70;

    merge(&firstValues, &secondValues);
    EXPECT_EQ(firstValues["servers"]["SERVER_1"]["info"]["status"], "Online");
    EXPECT_EQ(firstValues["servers"]["SERVER_1"]["info"]["publicIp"], "192.168.0.1");
    EXPECT_EQ(firstValues["servers"]["SERVER_1"]["load"]["cpuUsageP"], 50);
    EXPECT_EQ(firstValues["servers"]["SERVER_2"]["info"]["status"], "Online");
    EXPECT_EQ(firstValues["servers"]["SERVER_2"]["info"]["publicIp"], "192.168.0.2");
    EXPECT_EQ(firstValues["servers"]["SERVER_2"]["load"]["cpuUsageP"], 70);
}

static const QByteArray kAlarmsExample(R"json({
    "servers": {
        "SERVER_UUID_1": { "load": { "totalCpuUsageP": [ { "level": "error", "text": "CPU Usage is 95%" } ] } },
        "SERVER_UUID_2": { "availability": { "status": [ { "level": "error", "text": "Status is Offline" } ] } }
    },
    "cameras": {
        "CAMERA_UUID_1": { "primaryStream": { "targetFps": [ { "level": "warning", "text": "Actual FPS is 30" } ] } },
        "CAMERA_UUID_3": { "issues": { "offlineEvents": [ { "level": "warning", "text": "Offline Events is 2" } ] } }
    }
})json");

struct Counts
{
    const size_t types = 0;
    const size_t resources = 0;
    const size_t groups = 0;
    const size_t values = 0;
};

class ValuesPerformance: public testing::TestWithParam<Counts>
{
protected:
    SiteValues makeValues() const
    {
        const auto counts = GetParam();
        SiteValues values;
        for (size_t t = 0; t < counts.types; ++t)
        {
            auto& type = values[QStringLiteral("resource_type_%1").arg(t)];
            for (size_t r = 0; r < counts.resources; ++r)
            {
                auto& resource = type[QStringLiteral("resource_id_%1").arg(r)];
                for (size_t g = 0; g < counts.groups; ++g)
                {
                    auto& group = resource[QStringLiteral("group_id_%1").arg(g)];
                    for (size_t v = 0; v < counts.values; ++v)
                        group[QStringLiteral("parameter_value_id_%1").arg(r)] = Value((double) v);
                }
            }
        }
        return values;
    }
};

TEST_P(ValuesPerformance, Serialization)
{
    const auto values = makeValues();

    nx::utils::ElapsedTimer timer(nx::utils::ElapsedTimerState::started);
    const auto serialized = QJson::serialized(values);
    NX_INFO(this, "Serialized %1 bytes in %2", serialized.size(), timer.elapsed());

    SiteValues deserialized;
    timer.restart();
    EXPECT_TRUE(QJson::deserialize(serialized, &deserialized));
    NX_INFO(this, "Deserialized %1 bytes in %2", serialized.size(), timer.elapsed());
}

INSTANTIATE_TEST_SUITE_P(Metrics, ValuesPerformance, ::testing::Values(
   Counts{1, 100, 1, 5},
   Counts{5, 100, 5, 10},
   Counts{5, 1000, 5, 10},
   Counts{5, 10000, 5, 10}
));

TEST(Metrics, Alarms)
{
    SiteAlarms alarms;
    ASSERT_TRUE(QJson::deserialize(kAlarmsExample, &alarms));
    EXPECT_EQ(alarms.size(), 2);

    const auto s1 = alarms["servers"]["SERVER_UUID_1"]["load"]["totalCpuUsageP"];
    EXPECT_EQ(s1.size(), 1);
    EXPECT_EQ(s1[0].level, AlarmLevel::error);
    EXPECT_EQ(s1[0].text, "CPU Usage is 95%");

    const auto s2 = alarms["servers"]["SERVER_UUID_2"]["availability"]["status"];
    EXPECT_EQ(s2.size(), 1);
    EXPECT_EQ(s2[0].level, AlarmLevel::error);
    EXPECT_EQ(s2[0].text, "Status is Offline");

    const auto c1 = alarms["cameras"]["CAMERA_UUID_1"]["primaryStream"]["targetFps"];
    EXPECT_EQ(c1.size(), 1);
    EXPECT_EQ(c1[0].level, AlarmLevel::warning);
    EXPECT_EQ(c1[0].text, "Actual FPS is 30");

    const auto c2 = alarms["cameras"]["CAMERA_UUID_3"]["issues"]["offlineEvents"];
    EXPECT_EQ(c2.size(), 1);
    EXPECT_EQ(c2[0].level, AlarmLevel::warning);
    EXPECT_EQ(c2[0].text, "Offline Events is 2");
}

TEST(Metrics, Formatting)
{
    #define EXPECT_FORMAT(FORMAT, VALUE, RESULT) \
        EXPECT_EQ(makeFormatter(FORMAT)(Value(VALUE)), Value(RESULT))

    EXPECT_FORMAT("shortText", "hello", "hello");
    EXPECT_FORMAT("longText", "world", "world");
    EXPECT_FORMAT("text", "!", "!");

    EXPECT_FORMAT("number", 123.45, "123");

    EXPECT_FORMAT("", 100.0, "100");
    EXPECT_FORMAT("pixels", 100.0, "100 pixels");
    EXPECT_FORMAT("%", 0.15, "15%");

    EXPECT_FORMAT("KB", 100.0, "0.1 KB");
    EXPECT_FORMAT("KB", 5000.0, "4.88 KB");
    EXPECT_FORMAT("KB", 5000555.0, "4883 KB");

    EXPECT_FORMAT("MB", 100.0, "0 MB");
    EXPECT_FORMAT("MB", 6000666.0, "5.72 MB");
    EXPECT_FORMAT("MB", 6000666000.0, "5723 MB");

    EXPECT_FORMAT("KB/s", 5000555.0, "4883 KB/s");
    EXPECT_FORMAT("MPix/s", 6000666000.0, "6001 MPix/s");
    EXPECT_FORMAT("Gbit/s", 7000777000.0, "56 Gbit/s");

    EXPECT_FORMAT("duration", ((11 * 60 + 12) * 60) + 13, "11:12:13");
    EXPECT_FORMAT("duration", (((2 * 24 + 01) * 60 + 02) * 60) + 03, "2d 01:02:03");

    EXPECT_FORMAT("durationDh", ((11 * 60 + 12) * 60) + 13, "11h");
    EXPECT_FORMAT("durationDh", (((2 * 24 + 01) * 60 + 02) * 60) + 03, "2d 1h");

    #undef EXPECT_FORMAT
}

} // namespace nx::vms::api::metrics::test
