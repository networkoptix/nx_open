// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <QtCore/QFile>

#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/analytics/taxonomy/state_helper.h>
#include <nx/analytics/taxonomy/support/test_resource_support_proxy.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/common/test_support/resource/analytics_engine_resource_mock.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>

namespace nx::analytics::taxonomy {

using namespace nx::vms::common;

using Descriptors = nx::vms::api::analytics::Descriptors;

static const QString kInputDataKey = "inputData";
static const QString kExpectedDataKey = "expectedData";
static const QString kAnalyticsEnginesKey = "analyticsEngines";
static const QString kDevicesKey = "devices";
static const QString kTaxonomyKey = "taxonomy";
static const QString kTestsKey = "tests";
static const QString kIdKey = "id";

static QnUuid toUuid(const QString& str)
{
    return QnUuid::fromArbitraryData(str);
}

struct TestScope
{
    QString engineId;
    QString groupId;
};
#define TestScope_Fields (engineId)(groupId)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TestScope, (json), TestScope_Fields, (brief, true));

struct TestDescriptor
{
    QString id;
    QString name;
    std::vector<TestScope> scopes;
};
#define TestDescriptor_Fields (id)(name)(scopes)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TestDescriptor, (json), TestDescriptor_Fields, (brief, true));

struct TestDescriptors
{
    std::vector<TestDescriptor> groupDescriptors;
    std::vector<TestDescriptor> eventTypeDescriptors;
    std::vector<TestDescriptor> objectTypeDescriptors;
    std::vector<TestDescriptor> engineDescriptors;

    std::shared_ptr<AbstractState> toState() const
    {
        Descriptors descriptors;

        for (const auto& testGroupDescriptor: groupDescriptors)
        {
            nx::vms::api::analytics::GroupDescriptor descriptor;
            descriptor.id = testGroupDescriptor.id;
            descriptor.name = testGroupDescriptor.name;
            descriptors.groupDescriptors.emplace(testGroupDescriptor.id, std::move(descriptor));
        }

        for (const auto& testEventTypeDescriptor: eventTypeDescriptors)
        {
            nx::vms::api::analytics::EventTypeDescriptor descriptor;
            descriptor.id = testEventTypeDescriptor.id;
            descriptor.name = testEventTypeDescriptor.name;

            for (const auto& testScope: testEventTypeDescriptor.scopes)
            {
                nx::vms::api::analytics::DescriptorScope scope;
                scope.engineId = QnUuid::fromArbitraryData(testScope.engineId);
                scope.groupId = testScope.groupId;
                descriptor.scopes.insert(std::move(scope));
            }

            descriptors.eventTypeDescriptors.emplace(
                testEventTypeDescriptor.id, std::move(descriptor));
        }

        for (const auto& testObjectTypeDescriptor: objectTypeDescriptors)
        {
            nx::vms::api::analytics::ObjectTypeDescriptor descriptor;
            descriptor.id = testObjectTypeDescriptor.id;
            descriptor.name = testObjectTypeDescriptor.name;

            for (const auto& testScope: testObjectTypeDescriptor.scopes)
            {
                nx::vms::api::analytics::DescriptorScope scope;
                scope.engineId = QnUuid::fromArbitraryData(testScope.engineId);
                scope.groupId = testScope.groupId;
                descriptor.scopes.insert(std::move(scope));
            }

            descriptors.objectTypeDescriptors.emplace(
                testObjectTypeDescriptor.id, std::move(descriptor));
        }

        for (const auto& testEngineDescriptor: engineDescriptors)
        {
            nx::vms::api::analytics::EngineDescriptor descriptor;
            descriptor.id = toUuid(testEngineDescriptor.id);
            descriptor.name = testEngineDescriptor.name;
            descriptors.engineDescriptors.emplace(descriptor.id, descriptor);
        }

        return StateCompiler::compile(
            descriptors, std::make_unique<TestResourceSupportProxy>()).state;
    }
};
#define TestDescriptors_Fields \
    (groupDescriptors)\
    (eventTypeDescriptors)\
    (objectTypeDescriptors)\
    (engineDescriptors)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TestDescriptors, (json), TestDescriptors_Fields, (brief, true));

struct TestScopedEntity
{
    QString engineId;
    QString groupId;
    QString entityId;
};
#define TestScopedEntity_Fields (engineId)(groupId)(entityId)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    TestScopedEntity, (json), TestScopedEntity_Fields, (brief, true));

struct TestGroupScope
{
    QString group;
    std::vector<QString> entities;
};
#define TestGroupScope_Fields (group)(entities)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    TestGroupScope, (json), TestGroupScope_Fields, (brief, true));

struct TestEngineScope
{
    QString engine;
    std::vector<TestGroupScope> groups;
};
#define TestEngineScope_Fields (engine)(groups)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    TestEngineScope, (json), TestEngineScope_Fields, (brief, true));

class StateHelperTest: public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        QFile file(":/content/taxonomy/state_helper_test.json");
        if (!file.open(QFile::ReadOnly))
            FAIL();

        QJsonObject fullData;
        if (!QJson::deserialize(file.readAll(), &fullData))
            FAIL();

        if (!fullData.contains(kInputDataKey) || !fullData[kInputDataKey].isObject())
            FAIL();

        const QJsonObject inputData = fullData[kInputDataKey].toObject();
        if (!inputData.contains(kTaxonomyKey) || !inputData[kTaxonomyKey].isObject())
            FAIL();

        TestDescriptors descriptors;
        if (!QJson::deserialize(inputData[kTaxonomyKey], &descriptors))
            FAIL();

        const std::shared_ptr<AbstractState> state = descriptors.toState();
        if (!state)
            FAIL();

        s_stateHelper = std::make_unique<StateHelper>(state);

        if (!inputData.contains(kAnalyticsEnginesKey)
            || !inputData[kAnalyticsEnginesKey].isArray())
        {
            FAIL();
        }

        if (!inputData.contains(kTestsKey) || !inputData[kTestsKey].isObject())
            FAIL();

        if (!fullData.contains(kExpectedDataKey) || !fullData[kExpectedDataKey].isObject())
            FAIL();

        const QJsonArray analyticsEngines = inputData[kAnalyticsEnginesKey].toArray();
        for (const QJsonValue& value: analyticsEngines)
            createAnalyticsEngine(value);

        const QJsonArray devices = inputData[kDevicesKey].toArray();
        for (const QJsonValue& value: devices)
            createDevice(value);

        s_tests = inputData[kTestsKey].toObject();

        s_expectedData = fullData[kExpectedDataKey].toObject();
    }

    static void createAnalyticsEngine(const QJsonValue& value)
    {
        if (!value.isObject())
            return;

        const QJsonObject engineParameters = value.toObject();

        static const QString kIsDeviceDependentKey = "isDeviceDependent";
        static const QString kAnalyticsEventTypeIdsKey = "analyticsEventTypeIds";
        static const QString kAnalyticsObjectTypeIdsKey = "analyticsObjectTypeIds";

        if (!engineParameters.contains(kIdKey) || !engineParameters[kIdKey].isString())
            return;

        const QString engineStringId = engineParameters[kIdKey].toString();
        AnalyticsEngineResourceMockPtr analyticsEngine = AnalyticsEngineResourceMockPtr::create();
        analyticsEngine->setIdUnsafe(toUuid(engineStringId));
        analyticsEngine->setIsDeviceDependent(
            engineParameters[kIsDeviceDependentKey].toBool(/*defaultValue*/ false));

        std::set<QString> analyticsEventTypeIds;
        for (const QJsonValue& value: engineParameters[kAnalyticsEventTypeIdsKey].toArray())
        {
            if (const QString eventTypeId = value.toString(); !eventTypeId.isEmpty())
                analyticsEventTypeIds.insert(eventTypeId);
        }

        std::set<QString> analyticsObjectTypeIds;
        for (const QJsonValue& value: engineParameters[kAnalyticsObjectTypeIdsKey].toArray())
        {
            if (const QString objectTypeId = value.toString(); !objectTypeId.isEmpty())
                analyticsObjectTypeIds.insert(objectTypeId);
        }

        analyticsEngine->setAnalyticsEventTypeIds(std::move(analyticsEventTypeIds));
        analyticsEngine->setAnalyticsObjectTypeIds(std::move(analyticsObjectTypeIds));

        s_analyticsEngines[engineStringId] = analyticsEngine;
    }

    // Should be called after s_analyticsEngines is initialized.
    static void createDevice(const QJsonValue& value)
    {
        if (!value.isObject())
            return;

        const QJsonObject deviceParameters = value.toObject();

        static const QString kEnabledAnalyticsEnginesKey = "enabledAnalyticsEngines";
        static const QString kCompatibleAnalyticsEnginesKey = "compatibleAnalyticsEngines";
        static const QString kSupportedEventTypesKey = "supportedEventTypes";
        static const QString kSupportedObjectTypesKey = "supportedObjectTypes";

        CameraResourceStubPtr device = CameraResourceStubPtr::create();

        const QString deviceStringId = deviceParameters[kIdKey].toString();
        device->setIdUnsafe(toUuid(deviceStringId));

        std::set<QString> enabledAnalyticsEngineStringIds;
        QJson::deserialize(
            deviceParameters[kEnabledAnalyticsEnginesKey], &enabledAnalyticsEngineStringIds);

        QnUuidSet enabledAnalyticsEngineIds;
        for (const QString& stringId: enabledAnalyticsEngineStringIds)
            enabledAnalyticsEngineIds.insert(toUuid(stringId));

        device->setEnabledAnalyticsEngines(std::move(enabledAnalyticsEngineIds));

        std::set<QString> compatibleAnalyticsEngineIds;
        QJson::deserialize(
            deviceParameters[kCompatibleAnalyticsEnginesKey], &compatibleAnalyticsEngineIds);

        AnalyticsEngineResourceList compatibleAnalyticsEngineResources;
        for (const QString& engineId: compatibleAnalyticsEngineIds)
        {
            if (const auto it = s_analyticsEngines.find(engineId); it != s_analyticsEngines.cend())
                compatibleAnalyticsEngineResources.push_back(it->second);
        }

        device->setCompatibleAnalyticsEngineResources(compatibleAnalyticsEngineResources);

        std::map<QString, std::set<QString>> eventTypesByEngineStringId;
        QJson::deserialize(deviceParameters[kSupportedEventTypesKey], &eventTypesByEngineStringId);

        QnVirtualCameraResource::AnalyticsEntitiesByEngine eventTypesByEngine;
        for (auto& [stringId, eventTypeIds]: eventTypesByEngineStringId)
            eventTypesByEngine.emplace(toUuid(stringId), std::move(eventTypeIds));

        device->setSupportedEventTypes(QMap<QnUuid, std::set<QString>>(eventTypesByEngine));

        std::map<QString, std::set<QString>> objectTypesByEngineStringId;
        QJson::deserialize(
            deviceParameters[kSupportedObjectTypesKey], &objectTypesByEngineStringId);

        QnVirtualCameraResource::AnalyticsEntitiesByEngine objectTypesByEngine;
        for (auto& [stringId, objectTypeIds]: objectTypesByEngineStringId)
            objectTypesByEngine.emplace(toUuid(stringId), std::move(objectTypeIds));

        device->setSupportedObjectTypes(QMap<QnUuid, std::set<QString>>(objectTypesByEngine));

        s_devices[deviceStringId] = device;
    }

    void givenTestData(const QString& testName)
    {
        m_devices.clear();
        m_additionalEntities.clear();
        m_expectedEntities.clear();
        m_expectedEntityTree.clear();

        if (!s_tests.contains(testName) || !s_expectedData.contains(testName)
            || !s_tests[testName].isObject())
        {
            FAIL();
        }

        const QJsonObject testParameters = s_tests[testName].toObject();

        if (testParameters.contains(kDevicesKey))
        {
            if (!testParameters[kDevicesKey].isArray())
                FAIL();

            for (const QJsonValue& value: testParameters[kDevicesKey].toArray())
            {
                if (!value.isString())
                    continue;

                if (const auto it = s_devices.find(value.toString()); it != s_devices.cend())
                    m_devices.push_back(it->second);
            }
        }

        static const QString kAdditionalEntitiesKey = "additionalEntities";
        if (testParameters.contains(kAdditionalEntitiesKey))
            QJson::deserialize(testParameters[kAdditionalEntitiesKey], &m_additionalEntities);

        QJson::deserialize(s_expectedData[testName], &m_expectedEntities);
        QJson::deserialize(s_expectedData[testName], &m_expectedEntityTree);
    }

//-------------------------------------------------------------------------------------------------

private:
    template<typename EntityType>
    void thenEntitySetIsCorrect(
        std::function<std::map<QString, EntityType*>()> actualEntitySetGetter)
    {
        const std::map<QString, EntityType*> actualEntityTypes = actualEntitySetGetter();

        std::set<QString> actualEntityTypeIds;
        for (const auto& [_, entityType]: actualEntityTypes)
            actualEntityTypeIds.insert(entityType->id());

        ASSERT_EQ(actualEntityTypeIds, m_expectedEntities);
    }

    template<typename EntityType>
    void thenEntityTreeIsCorrect(
        std::function<std::vector<EngineScope<EntityType>>()> actualEntityTreeGetter)
    {
        const std::vector<EngineScope<EntityType>> actualEntityTree = actualEntityTreeGetter();

        ASSERT_EQ(m_expectedEntityTree.size(), actualEntityTree.size());
        for (int i = 0; i < actualEntityTree.size(); ++i)
        {
            const EngineScope<EntityType> actualEngineScope = actualEntityTree[i];
            const TestEngineScope expectedEngineScope = m_expectedEntityTree[i];

            ASSERT_EQ(
                QnUuid::fromStringSafe(actualEngineScope.engine->id()),
                toUuid(m_expectedEntityTree[i].engine));

            ASSERT_EQ(actualEngineScope.groups.size(), expectedEngineScope.groups.size());
            for (int j = 0; j < actualEngineScope.groups.size(); ++j)
            {
                const GroupScope<EntityType> actualGroupScope = actualEngineScope.groups[j];
                const TestGroupScope expectedGroupScope = expectedEngineScope.groups[j];

                if (actualGroupScope.group)
                    ASSERT_EQ(actualGroupScope.group->id(), expectedGroupScope.group);
                else
                    ASSERT_TRUE(expectedGroupScope.group.isEmpty());

                ASSERT_EQ(actualGroupScope.entities.size(), expectedGroupScope.entities.size());
                for (int k = 0; k < actualGroupScope.entities.size(); ++k)
                {
                    const EntityType* actualEntityType = actualGroupScope.entities[k];
                    const QString expectedEntityId = expectedGroupScope.entities[k];

                    ASSERT_EQ(actualEntityType->id(), expectedEntityId);
                }
            }
        }
    }

protected:
    void thenSupportedEventTypesAreCorrect()
    {
        ASSERT_EQ(m_devices.size(), 1);
        thenEntitySetIsCorrect<AbstractEventType>(
            [this]() { return s_stateHelper->supportedEventTypes(m_devices[0]); });
    }

    void thenSupportedObjectTypesAreCorrect()
    {
        ASSERT_EQ(m_devices.size(), 1);
        thenEntitySetIsCorrect<AbstractObjectType>(
            [this]() { return s_stateHelper->supportedObjectTypes(m_devices[0]); });
    }

    void thenSupportedEventTypeUnionIsCorrect()
    {
        thenEntitySetIsCorrect<AbstractEventType>(
            [this]() { return s_stateHelper->supportedEventTypeUnion(m_devices); });
    }

    void thenSupportedObjectTypeUnionIsCorrect()
    {
        thenEntitySetIsCorrect<AbstractObjectType>(
            [this]() { return s_stateHelper->supportedObjectTypeUnion(m_devices); });
    }

    void thenSupportedEventTypeIntersectionIsCorrect()
    {
        thenEntitySetIsCorrect<AbstractEventType>(
            [this]() { return s_stateHelper->supportedEventTypeIntersection(m_devices); });
    }

    void thenSupportedObjectTypeIntersectionIsCorrect()
    {
        thenEntitySetIsCorrect<AbstractObjectType>(
            [this]() { return s_stateHelper->supportedObjectTypeIntersection(m_devices); });
    }

    void thenCompatibleEventTypesAreCorrect()
    {
        ASSERT_EQ(m_devices.size(), 1);
        thenEntitySetIsCorrect<AbstractEventType>(
            [this]() { return s_stateHelper->compatibleEventTypes(m_devices[0]); });
    }

    void thenCompatibleObjectTypesAreCorrect()
    {
        ASSERT_EQ(m_devices.size(), 1);
        thenEntitySetIsCorrect<AbstractObjectType>(
            [this]() { return s_stateHelper->compatibleObjectTypes(m_devices[0]); });
    }

    void thenCompatibleEventTypeUnionIsCorrect()
    {
        thenEntitySetIsCorrect<AbstractEventType>(
            [this]() { return s_stateHelper->compatibleEventTypeUnion(m_devices); });
    }

    void thenCompatibleObjectTypeUnionIsCorrect()
    {
        thenEntitySetIsCorrect<AbstractObjectType>(
            [this]() { return s_stateHelper->compatibleObjectTypeUnion(m_devices); });
    }

    void thenCompatibleEventTypeIntersectionIsCorrect()
    {
        thenEntitySetIsCorrect<AbstractEventType>(
            [this]() { return s_stateHelper->compatibleEventTypeIntersection(m_devices); });
    }

    void thenCompatibleObjectTypeIntersectionIsCorrect()
    {
        thenEntitySetIsCorrect<AbstractObjectType>(
            [this]() { return s_stateHelper->compatibleObjectTypeIntersection(m_devices); });
    }

    void thenSupportedEventTypeTreeUnionIsCorrect()
    {
        thenEntityTreeIsCorrect<AbstractEventType>(
            [this]() { return s_stateHelper->supportedEventTypeTreeUnion(m_devices); });
    }

    void thenSupportedObjectTypeTreeUnionIsCorrect()
    {
        thenEntityTreeIsCorrect<AbstractObjectType>(
            [this]() { return s_stateHelper->supportedObjectTypeTreeUnion(m_devices); });
    }

    void thenCompatibleEventTypeTreeUnionIsCorrect()
    {
        thenEntityTreeIsCorrect<AbstractEventType>(
            [this]() { return s_stateHelper->compatibleEventTypeTreeUnion(m_devices); });
    }

    void thenCompatibleObjectTypeTreeUnionIsCorrect()
    {
        thenEntityTreeIsCorrect<AbstractObjectType>(
            [this]() { return s_stateHelper->compatibleObjectTypeTreeUnion(m_devices); });
    }

    void thenSupportedEventTypeTreeIntersectionIsCorrect()
    {
        thenEntityTreeIsCorrect<AbstractEventType>(
            [this]() { return s_stateHelper->supportedEventTypeTreeIntersection(m_devices); });
    }

    void thenSupportedObjectTypeTreeIntersectionIsCorrect()
    {
        thenEntityTreeIsCorrect<AbstractObjectType>(
            [this]() { return s_stateHelper->supportedObjectTypeTreeIntersection(m_devices); });
    }

    void thenCompatibleEventTypeTreeIntersectionIsCorrect()
    {
        thenEntityTreeIsCorrect<AbstractEventType>(
            [this]() { return s_stateHelper->compatibleEventTypeTreeIntersection(m_devices); });
    }

    void thenCompatibleObjectTypeTreeIntersectionIsCorrect()
    {
        thenEntityTreeIsCorrect<AbstractObjectType>(
            [this]() { return s_stateHelper->compatibleObjectTypeTreeIntersection(m_devices); });
    }

protected:
    static std::unique_ptr<StateHelper> s_stateHelper;
    static std::map<QString, AnalyticsEngineResourcePtr> s_analyticsEngines;
    static std::map<QString, QnVirtualCameraResourcePtr> s_devices;
    static QJsonObject s_tests;
    static QJsonObject s_expectedData;

private:
    QnVirtualCameraResourceList m_devices;
    std::vector<TestScopedEntity> m_additionalEntities;
    std::set<QString> m_expectedEntities;
    std::vector<TestEngineScope> m_expectedEntityTree;
};

std::unique_ptr<StateHelper> StateHelperTest::s_stateHelper = nullptr;
std::map<QString, AnalyticsEngineResourcePtr> StateHelperTest::s_analyticsEngines = {};
std::map<QString, QnVirtualCameraResourcePtr> StateHelperTest::s_devices = {};
QJsonObject StateHelperTest::s_tests = QJsonObject();
QJsonObject StateHelperTest::s_expectedData = QJsonObject();

//-------------------------------------------------------------------------------------------------

TEST_F(StateHelperTest, supportedEventTypes)
{
    givenTestData("supportedEventTypes");
    thenSupportedEventTypesAreCorrect();
}

TEST_F(StateHelperTest, supportedEventTypeUnion)
{
    givenTestData("supportedEventTypeUnion");
    thenSupportedEventTypeUnionIsCorrect();
}

TEST_F(StateHelperTest, supportedEventTypeIntersection)
{
    givenTestData("supportedEventTypeIntersection");
    thenSupportedEventTypeIntersectionIsCorrect();
}

TEST_F(StateHelperTest, supportedEventTypeTreeUnion)
{
    givenTestData("supportedEventTypeTreeUnion");
    thenSupportedEventTypeTreeUnionIsCorrect();
}

TEST_F(StateHelperTest, supportedEventTypeTreeIntersection)
{
    givenTestData("supportedEventTypeTreeIntersection");
    thenSupportedEventTypeTreeIntersectionIsCorrect();
}

//-------------------------------------------------------------------------------------------------

TEST_F(StateHelperTest, compatibleEventTypes)
{
    givenTestData("compatibleEventTypes");
    thenCompatibleEventTypesAreCorrect();
}

TEST_F(StateHelperTest, compatibleEventTypeUnion)
{
    givenTestData("compatibleEventTypeUnion");
    thenCompatibleEventTypeUnionIsCorrect();
}

TEST_F(StateHelperTest, compatibleEventTypeIntersection)
{
    givenTestData("compatibleEventTypeIntersection");
    thenCompatibleEventTypeIntersectionIsCorrect();
}

TEST_F(StateHelperTest, compatibleEventTypeTreeUnion)
{
    givenTestData("compatibleEventTypeTreeUnion");
    thenCompatibleEventTypeTreeUnionIsCorrect();
}

TEST_F(StateHelperTest, compatibleEventTypeTreeIntersection)
{
    givenTestData("compatibleEventTypeTreeIntersection");
    thenCompatibleEventTypeTreeIntersectionIsCorrect();
}

//-------------------------------------------------------------------------------------------------

TEST_F(StateHelperTest, supportedObjectTypes)
{
    givenTestData("supportedObjectTypes");
    thenSupportedObjectTypesAreCorrect();
}

TEST_F(StateHelperTest, supportedObjectTypeUnion)
{
    givenTestData("supportedObjectTypeUnion");
    thenSupportedObjectTypeUnionIsCorrect();
}

TEST_F(StateHelperTest, supportedObjectTypeIntersection)
{
    givenTestData("supportedObjectTypeIntersection");
    thenSupportedObjectTypeIntersectionIsCorrect();
}

TEST_F(StateHelperTest, supportedObjectTypeTreeUnion)
{
    givenTestData("supportedObjectTypeTreeUnion");
    thenSupportedObjectTypeTreeUnionIsCorrect();
}

TEST_F(StateHelperTest, supportedObjectTypeTreeIntersection)
{
    givenTestData("supportedObjectTypeTreeIntersection");
    thenSupportedObjectTypeTreeIntersectionIsCorrect();
}

//-------------------------------------------------------------------------------------------------

TEST_F(StateHelperTest, compatibleObjectTypes)
{
    givenTestData("compatibleObjectTypes");
    thenCompatibleObjectTypesAreCorrect();
}

TEST_F(StateHelperTest, compatibleObjectTypeUnion)
{
    givenTestData("compatibleObjectTypeUnion");
    thenCompatibleObjectTypeUnionIsCorrect();
}

TEST_F(StateHelperTest, compatibleObjectTypeIntersection)
{
    givenTestData("compatibleObjectTypeIntersection");
    thenCompatibleObjectTypeIntersectionIsCorrect();
}

TEST_F(StateHelperTest, compatibleObjectTypeTreeUnion)
{
    givenTestData("compatibleObjectTypeTreeUnion");
    thenCompatibleObjectTypeTreeUnionIsCorrect();
}

TEST_F(StateHelperTest, compatibleObjectTypeTreeIntersection)
{
    givenTestData("compatibleObjectTypeTreeIntersection");
    thenCompatibleObjectTypeTreeIntersectionIsCorrect();
}

} // namespace nx::analytics::taxonomy
