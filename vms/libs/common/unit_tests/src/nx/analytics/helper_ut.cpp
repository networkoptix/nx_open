#include <gtest/gtest.h>

#include <memory>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/access/access_types.h>
#include <nx/core/resource/device_mock.h>
#include <nx/core/resource/server_mock.h>

#include <nx/fusion/model_functions.h>

#include <nx/analytics/descriptor_manager.h>
#include <nx/analytics/properties.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

namespace {

const QString kEngineName("Engine");

QnUuid makeEngineId(const QString& pluginId, int engineIndex)
{
    return QnUuid::fromArbitraryData(pluginId + QString::number(engineIndex));
}

QnUuid makeDeviceId(const QString deviceName)
{
    return QnUuid::fromArbitraryData(deviceName);
}

QString makeEventId(int index)
{
    return QString("event.%1.id").arg(index);
}

QString makeEventName(int index)
{
    return QString("event.%1.name").arg(index);
}

QSet<QString> makeEventIds(const std::vector<int>& indices)
{
    QSet<QString> eventIds;
    for (const auto& index: indices)
        eventIds.insert(makeEventId(index));

    return eventIds;
}

QSet<QString> extractEventTypeIds(const ScopedEventTypeIds& scopedEventTypeIds)
{
    QSet<QString> result;
    MapHelper::forEach(
        scopedEventTypeIds,
        [&result](const auto& /*key*/, const auto& eventTypeIds)
        {
            for (const auto& eventTypeId: eventTypeIds)
            result.insert(eventTypeId);
        });

    return result;
}

QnMediaServerResourcePtr makeServer(QnCommonModule* commonModule)
{
    QnMediaServerResourcePtr server(new nx::core::resource::ServerMock(commonModule));
    server->setId(QnUuid::createUuid());
    server->setResourcePool(commonModule->resourcePool());
    server->setStatus(Qn::Online);
    return server;
};

auto pluginDescriptors(const QnResourcePtr& resource)
{
    return QJson::deserialized<std::map<QString, PluginDescriptor>>(
        resource->getProperty(kPluginDescriptorsProperty).toUtf8());
}

auto engineDescriptors(const QnResourcePtr& resource)
{
    return QJson::deserialized<std::map<QnUuid, EngineDescriptor>>(
        resource->getProperty(kEngineDescriptorsProperty).toUtf8());
}

auto groupDescriptors(const QnResourcePtr& resource)
{
    return QJson::deserialized<std::map<QString, GroupDescriptor>>(
        resource->getProperty(kGroupDescriptorsProperty).toUtf8());
}

auto eventTypeDescriptors(const QnResourcePtr& resource)
{
    return QJson::deserialized<std::map<QString, EventTypeDescriptor>>(
        resource->getProperty(kEventTypeDescriptorsProperty).toUtf8());
}

auto objectTypeDescriptors(const QnResourcePtr& resource)
{
    return QJson::deserialized<std::map<QString, ObjectTypeDescriptor>>(
        resource->getProperty(kObjectTypeDescriptorsProperty).toUtf8());
}

auto actionTypeDescriptors(const QnResourcePtr& resource)
{
    return QJson::deserialized<
        MapHelper::NestedMap<std::map, QnUuid, QString, ActionTypeDescriptor>>(
        resource->getProperty(kActionTypeDescriptorsProperty).toUtf8());
}

const char* kBasicEngineManifest = R"json(
{
    "eventTypes": [
        {
            "id": "event.type.1",
            "name": { "value": "Event Type 1" }
        },
        {
            "id": "event.type.2",
            "name": {"value": "Event type 2"}
        },
        {
            "id": "event.type.3",
            "name": {"value": "Event type 3"},
            "groupId": "group.1"
        },
        {
            "id": "event.type.4",
            "name": {"value": "Event type 4"},
            "groupId": "group.2"
        }
    ],
    "objectTypes": [
        {
            "id": "object.type.1",
            "name": { "value": "Object Type 1" }
        },
        {
            "id": "object.type.2",
            "name": { "value": "Object Type 2" }
        }
    ],
    "groups": [
        {
            "id": "group.1",
            "name": { "value": "Group 1" }
        },
        {
            "id": "group.2",
            "name": { "value": "Group 2" }
        },
        {
            "id": "group.3",
            "name": { "value": "Group 3" }
        }
    ],
    "objectActions": [
        {
            "id": "object.action.1",
            "name": { "value": "Object Action 1" },
            "supportedObjectTypeIds": ["object.type.1"],
            "parametersModel": {
                "type": "Settings",
                "items": [
                    {
                        "type": "TextField",
                        "name": "test_text_field",
                        "caption": "Text Field Parameter",
                        "description": "A text field",
                        "defaultValue": "a text"
                    },
                    {
                        "type": "GroupBox",
                        "caption": "Parameter Group",
                        "items": [
                            {
                                "type": "SpinBox",
                                "caption": "SpinBox Parameter",
                                "name": "test_spin_box",
                                "defaultValue": 42,
                                "minValue": 0,
                                "maxValue": 100
                            },
                            {
                                "type": "DoubleSpinBox",
                                "caption": "DoubleSpinBox Parameter",
                                "name": "test_double_spin_box",
                                "defaultValue": 3.1415,
                                "minValue": 0.0,
                                "maxValue": 100.0
                            },
                            {
                                "type": "ComboBox",
                                "name": "test_combo_box",
                                "caption": "ComboBox Parameter",
                                "defaultValue": "value2",
                                "range": ["value1", "value2", "value3"]
                            },
                            {
                                "type": "Row",
                                "items": [
                                    {
                                        "type": "CheckBox",
                                        "caption": "CheckBox Parameter",
                                        "name": "test_check_box",
                                        "defaultValue": true,
                                        "value": true
                                    }
                                ]
                            }
                        ]
                    }
                ]
            }
        },
        {
            "id": "object.action.2",
            "name": { "value": "Object Action 2" },
            "supportedObjectTypeIds": [
                "object.type.2"
            ]
        }
    ]
}
)json";

const char* kBasicDeviceAgentManifest = R"json(
{
    "supportedEventTypeIds": [
        "event.type.1",
        "event.type.2"
    ],
    "supportedObjectTypeIds": [
        "object.type.1"
    ],
    "eventTypes": [
        {
            "id": "device_agent.event.type.1",
            "name": { "value": "DeviceAgent Event Type 1" }
        },
        {
            "id": "device_agent.event.type.2",
            "name": { "value": "DeviceAgent Event Type 2" },
        },
        {
            "id": "device_agent.event.type.2",
            "name": { "value": "DeviceAgent Event Type 2" },
            "groupId":
        }
    ],
    "objectTypes": [
        {
            "id": "device_agent.object.type.1",
            "name": { "value": "DeviceAgent Object Type 1" }
        },
        {
            "id": "device_agent.object.type.2",
            "name": { "value": "DeviceAgent Object Type 2" },
        }
    ],
    "groups": [
        {
            "id": "device_agent.group.1",
            "name": { "value": "DeviceAgent Group 1" }
        },
        {
            "id": "device_agent.group.2",
            "name": { "value": "DeviceAgent Group 2" }
        },
    ]
}
)json";

template<typename T>
void merge(T& target, const T& descriptor)
{
    ScopedMergeExecutor<T> merger;
    auto merged = merger(&target, &descriptor);
    if (merged)
        target = *merged;
};

template<typename Key, typename Value>
std::map<Key, Value> pickDescriptors(
    const std::map<Key,Value>& allDescriptors,
    const QSet<Key>& keys)
{
    std::map<Key, Value> result;
    for (const auto& key: keys)
    {
        if (allDescriptors.find(key) != allDescriptors.cend())
            result[key] = allDescriptors.at(key);
    }

    return result;
}

} // namespace

class HelperTest: public ::testing::Test
{

protected:
    virtual void SetUp() override
    {
        m_commonModule = std::make_unique<QnCommonModule>(
            /*clientMode*/ false,
            nx::core::access::Mode::direct);

        auto resourcePool = m_commonModule->resourcePool();
        m_ownServer = makeServer(m_commonModule.get());
        m_commonModule->setModuleGUID(m_ownServer->getId());
        resourcePool->addResource(m_ownServer);

        for (auto i = 0; i < 10; ++i)
        {
            auto serverResource = makeServer(m_commonModule.get());
            resourcePool->addResource(serverResource);
            m_servers.push_back(std::move(serverResource));
        }
    }

protected:

    struct DeviceAgentInfo
    {
        QnUuid deviceId;
        DeviceAgentManifest deviceAgentManifest;
    };

    struct EngineInfo
    {
        QnUuid engineId;
        EngineManifest engineManifest;
        std::vector<DeviceAgentInfo> deviceAgentInfos;
    };

    struct ManifestSet
    {
        PluginManifest pluginManifest;
        std::vector<EngineInfo> engineInfos;
    };

    struct ExpectedDescriptors
    {
        std::map<QString, PluginDescriptor> pluginDescriptors;
        std::map<QnUuid, EngineDescriptor> engineDescriptors;
        std::map<QString, GroupDescriptor> groupDescriptors;
        std::map<QString, EventTypeDescriptor> eventTypeDescriptors;
        std::map<QString, ObjectTypeDescriptor> objectTypeDescriptors;
        MapHelper::NestedMap<std::map, QnUuid, QString, ActionTypeDescriptor>
            actionTypeDescriptors;
    };

    PluginManifest givenManifestForPlugin(const QString& pluginId)
    {
        PluginManifest manifest;
        manifest.id = pluginId;
        manifest.name = QString("Plugin %1").arg(pluginId);

        return manifest;
    }

    EngineManifest givenManifestForEngine(const QnUuid& /*engineId*/)
    {
        return QJson::deserialized<EngineManifest>(QByteArray(kBasicEngineManifest));
    }

    DeviceAgentManifest givenManifestForDeviceAgent(
        const QnUuid& /*deviceId*/,
        const QnUuid& /*engineId*/)
    {
        return QJson::deserialized<DeviceAgentManifest>(QByteArray(kBasicDeviceAgentManifest));
    }

    ManifestSet givenPlugin(const QString& pluginId)
    {
        DescriptorManager descriptorManager(m_commonModule.get());
        const auto pluginManifest = givenManifestForPlugin("pluginId");
        descriptorManager.updateFromPluginManifest(pluginManifest);

        ManifestSet manifestSet;
        manifestSet.pluginManifest = pluginManifest;

        return manifestSet;
    }

    EngineInfo* givenEngine(
        const QString& pluginId,
        const QnUuid& engineId,
        ManifestSet* inOutManifestSet)
    {
        const auto engineManifest = givenManifestForEngine(engineId);

        EngineInfo engineInfo{ engineId, engineManifest };
        inOutManifestSet->engineInfos.push_back(engineInfo);

        DescriptorManager descriptorManager(m_commonModule.get());
        descriptorManager.updateFromEngineManifest(
            pluginId,
            engineId,
            kEngineName,
            engineManifest);

        return &inOutManifestSet->engineInfos.back();
    }

    void givenDeviceAgent(const QnUuid& deviceId, EngineInfo* inOutEngineInfo)
    {
        const auto engineId = inOutEngineInfo->engineId;
        const auto deviceAgentManifest = givenManifestForDeviceAgent(deviceId, engineId);

        inOutEngineInfo->deviceAgentInfos.push_back({deviceId, deviceAgentManifest});

        DescriptorManager descriptorManager(m_commonModule.get());
        descriptorManager.updateFromDeviceAgentManifest(deviceId, engineId, deviceAgentManifest);
    }

    void makeSureDescriptorsAreCorrectForManifests(const std::vector<ManifestSet>& manifestSets)
    {
        ExpectedDescriptors expected;
        for (const auto& manifestSet: manifestSets)
        {
            const auto pluginManifest = manifestSet.pluginManifest;
            expected.pluginDescriptors[pluginManifest.id] =
                PluginDescriptor(pluginManifest.id, pluginManifest.name);

            for (const auto& engineInfo: manifestSet.engineInfos)
                fillExpectedDescriptorsFromEngineInfo(pluginManifest.id, engineInfo, &expected);
        }

        ASSERT_EQ(expected.pluginDescriptors, pluginDescriptors(m_ownServer));
        ASSERT_EQ(expected.engineDescriptors, engineDescriptors(m_ownServer));
        ASSERT_EQ(expected.groupDescriptors, groupDescriptors(m_ownServer));
        ASSERT_EQ(expected.eventTypeDescriptors, eventTypeDescriptors(m_ownServer));
        ASSERT_EQ(expected.objectTypeDescriptors, objectTypeDescriptors(m_ownServer));
        ASSERT_EQ(expected.actionTypeDescriptors, actionTypeDescriptors(m_ownServer));
    }

    void fillExpectedDescriptorsFromEngineInfo(
        const QString& pluginId,
        const EngineInfo& engineInfo,
        ExpectedDescriptors* inOutExpectedDescriptors)
    {
        const auto& engineId = engineInfo.engineId;
        const auto& engineManifest = engineInfo.engineManifest;

        inOutExpectedDescriptors->engineDescriptors[engineId] =
            EngineDescriptor(engineId, kEngineName, pluginId);

        for (const auto& group: engineManifest.groups)
        {
            merge(
                MapHelper::get(inOutExpectedDescriptors->groupDescriptors, group.id),
                GroupDescriptor(engineId, group));
        }

        for (const auto& eventType : engineManifest.eventTypes)
        {
            merge(
                MapHelper::get(inOutExpectedDescriptors->eventTypeDescriptors, eventType.id),
                EventTypeDescriptor(engineId, eventType));
        }

        for (const auto& objectType : engineManifest.objectTypes)
        {
            merge(
                MapHelper::get(inOutExpectedDescriptors->objectTypeDescriptors, objectType.id),
                ObjectTypeDescriptor(engineId, objectType));
        }

        for (const auto& actionType : engineManifest.objectActions)
        {
            MapHelper::set(
                &inOutExpectedDescriptors->actionTypeDescriptors,
                engineId,
                actionType.id,
                ActionTypeDescriptor(engineId, actionType));
        }

        for (const auto& deviceAgentInfo: engineInfo.deviceAgentInfos)
        {
            fillExpectedDescriptorsFromDeviceAgentInfo(
                pluginId,
                engineId,
                deviceAgentInfo,
                inOutExpectedDescriptors);
        }
    }

    void fillExpectedDescriptorsFromDeviceAgentInfo(
        const QString& pluginId,
        const QnUuid& engineId,
        const DeviceAgentInfo& deviceAgentInfo,
        ExpectedDescriptors* inOutExpectedDescriptors)
    {
        const auto deviceAgentManifest = deviceAgentInfo.deviceAgentManifest;
        for (const auto& group: deviceAgentManifest.groups)
        {
            merge(
                MapHelper::get(inOutExpectedDescriptors->groupDescriptors, group.id),
                GroupDescriptor(engineId, group));
        }

        for (const auto& eventType: deviceAgentManifest.eventTypes)
        {
            merge(
                MapHelper::get(inOutExpectedDescriptors->eventTypeDescriptors, eventType.id),
                EventTypeDescriptor(engineId, eventType));
        }

        for (const auto& objectType: deviceAgentManifest.objectTypes)
        {
            merge(
                MapHelper::get(inOutExpectedDescriptors->objectTypeDescriptors, objectType.id),
                ObjectTypeDescriptor(engineId, objectType));
        }
    }

    void makeSureActionDescriptorsAreCleared()
    {
        ASSERT_EQ(
            (MapHelper::NestedMap<std::map, QnUuid, QString, ActionTypeDescriptor>()),
            actionTypeDescriptors(m_ownServer));
    }


    void givenServerWithEventDescriptors(
        QnMediaServerResourcePtr server,
        const QnUuid& engineId,
        const QString& group,
        int eventIndexRangeStart,
        int eventIndexRangeEnd,
        std::map<QString, EventTypeDescriptor>* expectedResult)
    {
        std::map<QString, EventTypeDescriptor> descriptorMap;
        for (auto i = eventIndexRangeStart; i <= eventIndexRangeEnd; ++i)
        {
            const QString eventId = makeEventId(i);
            QString eventName = makeEventName(i);

            descriptorMap[eventId] =
                EventTypeDescriptor(
                    engineId,
                    {{eventId, std::move(eventName)}, EventTypeFlags(), group});
        }

        server->setProperty(
            nx::analytics::kEventTypeDescriptorsProperty,
            QString::fromUtf8(QJson::serialized(descriptorMap)));

        MapHelper::merge(
            expectedResult,
            descriptorMap,
            ScopedMergeExecutor<EventTypeDescriptor>());
    }

    QnVirtualCameraResourcePtr makeDevice()
    {
        QnVirtualCameraResourcePtr device(new nx::core::resource::DeviceMock());
        device->setId(QnUuid::createUuid());
        device->setCommonModule(m_commonModule.get());

        return device;
    }

    QnVirtualCameraResourcePtr givenDeviceWhichSupportsEventTypes(QSet<QString> eventTypeIds)
    {
        auto device = makeDevice();
        device->setSupportedAnalyticsEventTypeIds(
            /*engineId*/ QnUuid::createUuid(),
            std::move(eventTypeIds));

        return device;
    }

protected:
    std::unique_ptr<QnCommonModule> m_commonModule;
    QnMediaServerResourcePtr m_ownServer;
    QnMediaServerResourceList m_servers;
};

TEST_F(HelperTest, updateFromPluginManifest)
{
    const auto manifestSet = givenPlugin("pluginId");
    const auto anotherManifestSet = givenPlugin("anotherPluginId");

    makeSureDescriptorsAreCorrectForManifests({manifestSet, anotherManifestSet});
}

TEST_F(HelperTest, updateFromEngineManifest)
{
    ManifestSet manifestSet = givenPlugin("pluginId");

    const auto pluginId = manifestSet.pluginManifest.id;
    const auto engineId = makeEngineId(pluginId, 0);
    EngineInfo* engineInfo = givenEngine(pluginId, engineId, &manifestSet);
    makeSureDescriptorsAreCorrectForManifests({manifestSet});

    const auto engineId2 = makeEngineId(pluginId, 1);
    EngineInfo* engineInfo2 = givenEngine(pluginId, engineId2, &manifestSet);
    makeSureDescriptorsAreCorrectForManifests({manifestSet});
}

TEST_F(HelperTest, updateFromDeviceAgentManifest)
{
    const auto pluginId("pluginId");
    ManifestSet manifestSet = givenPlugin(pluginId);

    EngineInfo* engineInfo = givenEngine(pluginId, makeEngineId(pluginId, 0), &manifestSet);
    givenDeviceAgent(makeDeviceId("device"), engineInfo);
    makeSureDescriptorsAreCorrectForManifests({manifestSet});
}

TEST_F(HelperTest, clearRuntimeInfo)
{
    ManifestSet manifestSet = givenPlugin("pluginId");

    const auto pluginId = manifestSet.pluginManifest.id;
    const auto engineId = makeEngineId(pluginId, 0);
    EngineInfo* engineInfo = givenEngine(pluginId, engineId, &manifestSet);

    const auto engineId2 = makeEngineId(pluginId, 1);
    EngineInfo* engineInfo2 = givenEngine(pluginId, engineId2, &manifestSet);

    DescriptorManager descriptorManager(m_commonModule.get());
    descriptorManager.clearRuntimeInfo();

    makeSureActionDescriptorsAreCleared();
}

TEST_F(HelperTest, eventTypes)
{
    static const QString kPluginId("pluginId");
    static const QnUuid kEngineId0 = makeEngineId(kPluginId, 0);
    static const QnUuid kEngineId1 = makeEngineId(kPluginId, 1);

    std::map<QString, EventTypeDescriptor> expectedResult;
    givenServerWithEventDescriptors(m_servers[1], kEngineId0, "group", 1, 3, &expectedResult);
    givenServerWithEventDescriptors(m_servers[2], kEngineId1, "group", 3, 4, &expectedResult);

    EventTypeDescriptorManager eventTypeDescriptorManager(m_commonModule.get());
    ASSERT_EQ(expectedResult, eventTypeDescriptorManager.descriptors());
}

// TODO: #dmishin fix tests below.

TEST_F(HelperTest, DISABLED_supportedEventTypes)
{
    static const QString kPluginId("pluginId");
    static const QnUuid kEngineId0 = makeEngineId(kPluginId, 0);
    static const QnUuid kEngineId1 = makeEngineId(kPluginId, 1);

    std::map<QString, EventTypeDescriptor> allEventDescriptors;
    givenServerWithEventDescriptors(m_servers[1], kEngineId0, "group", 1, 3, &allEventDescriptors);
    givenServerWithEventDescriptors(m_servers[2], kEngineId1, "group", 3, 4, &allEventDescriptors);

    static const auto eventTypeIds = makeEventIds({2, 4});
    const auto device = givenDeviceWhichSupportsEventTypes(eventTypeIds);

    const EventTypeDescriptorManager eventTypeDescriptorManager(m_commonModule.get());
    ASSERT_EQ(
        eventTypeDescriptorManager.supportedEventTypeDescriptors(device),
        pickDescriptors(allEventDescriptors, eventTypeIds));
}

TEST_F(HelperTest, DISABLED_supportedEventTypesUnion)
{
    static const QString kPluginId("pluginId");
    static const QnUuid kEngineId0 = makeEngineId(kPluginId, 0);
    static const QnUuid kEngineId1 = makeEngineId(kPluginId, 1);

    std::map<QString, EventTypeDescriptor> allEventDescriptors;
    givenServerWithEventDescriptors(m_servers[1], kEngineId0, "group", 1, 6, &allEventDescriptors);
    givenServerWithEventDescriptors(m_servers[2], kEngineId1, "group", 3, 5, &allEventDescriptors);

    auto eventTypeIds0 = makeEventIds({2, 4});
    static const auto eventTypeIds1 = makeEventIds({3, 6, 4, 9});
    const auto device0 = givenDeviceWhichSupportsEventTypes(eventTypeIds0);
    const auto device1 = givenDeviceWhichSupportsEventTypes(eventTypeIds1);

    const EventTypeDescriptorManager eventTypeDescriptorManager(m_commonModule.get());
    const auto scopedEventTypeIds = eventTypeDescriptorManager.supportedEventTypeIdsUnion(
        {device0, device1});

    const auto actualEventTypeIds = extractEventTypeIds(scopedEventTypeIds);
    ASSERT_EQ(
        actualEventTypeIds,
        eventTypeIds0.unite(eventTypeIds1));
}

TEST_F(HelperTest, DISABLED_supportedEventTypesIntersection)
{
    static const QString kPluginId("pluginId");
    static const QnUuid kEngineId0 = makeEngineId(kPluginId, 0);
    static const QnUuid kEngineId1 = makeEngineId(kPluginId, 1);

    std::map<QString, EventTypeDescriptor> allEventDescriptors;
    givenServerWithEventDescriptors(m_servers[1], kEngineId0, "group", 1, 6, &allEventDescriptors);
    givenServerWithEventDescriptors(m_servers[2], kEngineId1, "group", 3, 5, &allEventDescriptors);

    auto eventTypeIds0 = makeEventIds({ 2, 4 });
    static const auto eventTypeIds1 = makeEventIds({ 3, 6, 4, 9 });
    const auto device0 = givenDeviceWhichSupportsEventTypes(eventTypeIds0);
    const auto device1 = givenDeviceWhichSupportsEventTypes(eventTypeIds1);

    const EventTypeDescriptorManager eventTypeDescriptorManager(m_commonModule.get());
    const auto scopedEventTypeIds = eventTypeDescriptorManager.supportedEventTypeIdsIntersection(
        { device0, device1 });

    const auto actualEventTypeIds = extractEventTypeIds(scopedEventTypeIds);
    ASSERT_EQ(
        actualEventTypeIds,
        eventTypeIds0.intersect(eventTypeIds1));
}

} // namespace nx::analytics
