// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <core/resource/camera_resource.h>
#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/descriptor_container.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/client/core/analytics/analytics_filter_model.h>
#include <nx/vms/client/core/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/core/analytics/taxonomy/object_type.h>
#include <nx/vms/client/desktop/analytics/detectable_object_type_model.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

namespace nx::vms::client::desktop::analytics {
namespace test {

namespace {

using namespace nx::vms::api::analytics;

const nx::Uuid kEngineA("a1b1c1d1-0000-4000-8000-00000000000a");
const nx::Uuid kEngineB("a1b1c1d1-0000-4000-8000-00000000000b");
const std::string kPluginId = "test.plugin";
const QString kOnlyEngineAType = "test.object.onlyEngineA";
const QString kCommonType = "test.object.common";

ObjectTypeDescriptor objectTypeDescriptor(
    const QString& id, const std::string& name, const std::vector<nx::Uuid>& engineIds)
{
    ObjectTypeDescriptor result;
    result.id = id.toStdString();
    result.name = name;
    result.hasEverBeenSupported = true;
    for (const auto& engineId: engineIds)
    {
        DescriptorScope scope;
        scope.engineId = engineId;
        scope.hasTypeEverBeenSupportedInThisScope = true;
        result.scopes.insert(scope);
    }
    return result;
}

Descriptors makeDescriptors()
{
    Descriptors result;
    result.pluginDescriptors[kPluginId] = PluginDescriptor(kPluginId, "Test Plugin");

    const std::vector<std::pair<nx::Uuid, std::string>> engines{
        {kEngineA, "Engine A"}, {kEngineB, "Engine B"}};
    for (const auto& [id, name]: engines)
    {
        EngineDescriptor engine;
        engine.id = id;
        engine.name = name;
        engine.pluginId = kPluginId;
        result.engineDescriptors[id] = engine;
    }

    result.objectTypeDescriptors[kOnlyEngineAType.toStdString()] =
        objectTypeDescriptor(kOnlyEngineAType, "Only Engine A Type", {kEngineA});
    result.objectTypeDescriptors[kCommonType.toStdString()] =
        objectTypeDescriptor(kCommonType, "Common Type", {kEngineA, kEngineB});
    return result;
}

DeviceAgentManifest deviceAgentManifest(const std::vector<QString>& objectTypeIds)
{
    DeviceAgentManifest result;
    for (const auto& id: objectTypeIds)
        result.supportedObjectTypeIds.push_back(id.toStdString());
    return result;
}

} // namespace

/** The object type model over a filter model on a real taxonomy, as the right panel uses them. */
class DetectableObjectTypeModelEngineTest: public desktop::test::ContextBasedTest
{
    using base_type = desktop::test::ContextBasedTest;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        m_camera = addCamera();
        m_camera->setDeviceAgentManifest(
            kEngineA, deviceAgentManifest({kOnlyEngineAType, kCommonType}));
        m_camera->setDeviceAgentManifest(kEngineB, deviceAgentManifest({kCommonType}));

        systemContext()->analyticsDescriptorContainer()->updateDescriptorsForTests(
            makeDescriptors());

        m_taxonomyManager = std::make_unique<core::analytics::TaxonomyManager>(systemContext());
        m_filterModel = std::make_unique<core::analytics::taxonomy::AnalyticsFilterModel>(
            m_taxonomyManager.get());
        m_model = std::make_unique<DetectableObjectTypeModel>(m_filterModel.get());
        m_filterModel->setSelectedDevices(QnVirtualCameraResourceSet{m_camera});
    }

    virtual void TearDown() override
    {
        m_model.reset();
        m_filterModel.reset();
        m_taxonomyManager.reset();
        m_camera.reset();
        base_type::TearDown();
    }

    nx::analytics::taxonomy::AbstractEngine* engine(const nx::Uuid& id) const
    {
        for (const auto engine: m_taxonomyManager->currentTaxonomy()->engines())
        {
            if (nx::Uuid::fromStringSafe(engine->id()) == id)
                return engine;
        }
        return nullptr;
    }

    core::analytics::taxonomy::ObjectType* lookup(const QString& id) const
    {
        return m_model->sourceModel()->objectTypeById(id);
    }

    bool isCurrentRootObjectType(const core::analytics::taxonomy::ObjectType* objectType) const
    {
        const auto objectTypes = m_filterModel->objectTypes();
        return objectType && std::ranges::find(objectTypes, objectType) != objectTypes.cend();
    }

protected:
    QnVirtualCameraResourcePtr m_camera;
    std::unique_ptr<core::analytics::TaxonomyManager> m_taxonomyManager;
    std::unique_ptr<core::analytics::taxonomy::AnalyticsFilterModel> m_filterModel;
    std::unique_ptr<DetectableObjectTypeModel> m_model;
};

TEST_F(DetectableObjectTypeModelEngineTest, objectTypeLookupFollowsEngineSwitch)
{
    ASSERT_NE(engine(kEngineA), nullptr);
    ASSERT_NE(engine(kEngineB), nullptr);

    ASSERT_NE(lookup(kOnlyEngineAType), nullptr);
    ASSERT_NE(lookup(kCommonType), nullptr);

    m_model->setEngine(engine(kEngineA));
    ASSERT_NE(lookup(kOnlyEngineAType), nullptr);
    ASSERT_NE(lookup(kCommonType), nullptr);

    m_model->setEngine(engine(kEngineB));
    ASSERT_EQ(lookup(kOnlyEngineAType), nullptr);
    const auto commonType = lookup(kCommonType);
    ASSERT_TRUE(isCurrentRootObjectType(commonType));

    int notificationCount = 0;
    QObject context;
    QObject::connect(m_filterModel.get(),
        &core::analytics::taxonomy::AnalyticsFilterModel::objectTypesChanged,
        &context,
        [&]() { ++notificationCount; });

    m_model->setEngine(engine(kEngineB));
    ASSERT_EQ(notificationCount, 0);
    ASSERT_EQ(lookup(kCommonType), commonType);
    ASSERT_EQ(lookup(kOnlyEngineAType), nullptr);

    m_model->setEngine(engine(kEngineA));
    ASSERT_TRUE(isCurrentRootObjectType(lookup(kOnlyEngineAType)));

    m_model->setEngine(engine(kEngineB));
    m_model->setEngine(nullptr);
    ASSERT_TRUE(isCurrentRootObjectType(lookup(kOnlyEngineAType)));
}

TEST_F(DetectableObjectTypeModelEngineTest, engineSwitchNotifiesSynchronouslyWithUpdatedLookup)
{
    m_model->setEngine(engine(kEngineA));
    ASSERT_NE(lookup(kOnlyEngineAType), nullptr);

    int notificationCount = 0;
    nx::analytics::taxonomy::AbstractEngine* selectedEngine = nullptr;
    bool removedTypeFound = true;
    bool commonTypeIsCurrent = false;
    QObject context;
    QObject::connect(m_filterModel.get(),
        &core::analytics::taxonomy::AnalyticsFilterModel::objectTypesChanged,
        &context,
        [&]()
        {
            ++notificationCount;
            selectedEngine = m_filterModel->selectedEngine();
            removedTypeFound = lookup(kOnlyEngineAType) != nullptr;
            commonTypeIsCurrent = isCurrentRootObjectType(lookup(kCommonType));
        });

    m_model->setEngine(engine(kEngineB));

    ASSERT_EQ(notificationCount, 1);
    ASSERT_EQ(selectedEngine, engine(kEngineB));
    ASSERT_FALSE(removedTypeFound);
    ASSERT_TRUE(commonTypeIsCurrent);
}

TEST_F(DetectableObjectTypeModelEngineTest, setEngineAndFindObjectTypeLooksUpAmongNewEngineTypes)
{
    m_model->setEngine(engine(kEngineA));

    ASSERT_EQ(
        m_model->setEngineAndFindObjectType(engine(kEngineB), []() { return kOnlyEngineAType; }),
        nullptr);
    ASSERT_TRUE(isCurrentRootObjectType(
        m_model->setEngineAndFindObjectType(engine(kEngineA), []() { return kOnlyEngineAType; })));
    ASSERT_TRUE(isCurrentRootObjectType(
        m_model->setEngineAndFindObjectType(engine(kEngineB), []() { return kCommonType; })));
}

TEST_F(DetectableObjectTypeModelEngineTest, setEngineAndFindObjectTypeRequestsIdAfterSwitch)
{
    m_model->setEngine(engine(kEngineA));

    // Both engines have the type, so only an id requested after the switch is not found.
    QString selectedTypeId = kCommonType;
    QObject context;
    QObject::connect(m_filterModel.get(),
        &core::analytics::taxonomy::AnalyticsFilterModel::objectTypesChanged,
        &context,
        [&]() { selectedTypeId.clear(); });

    ASSERT_EQ(
        m_model->setEngineAndFindObjectType(engine(kEngineB), [&]() { return selectedTypeId; }),
        nullptr);
    ASSERT_TRUE(selectedTypeId.isEmpty());
}

TEST_F(
    DetectableObjectTypeModelEngineTest, setEngineAndFindObjectTypeWithoutEngineLooksUpAllEngines)
{
    m_model->setEngine(engine(kEngineB));
    ASSERT_EQ(lookup(kOnlyEngineAType), nullptr);

    ASSERT_TRUE(isCurrentRootObjectType(
        m_model->setEngineAndFindObjectType(nullptr, []() { return kOnlyEngineAType; })));
    ASSERT_EQ(m_filterModel->selectedEngine(), nullptr);
}

TEST_F(DetectableObjectTypeModelEngineTest, setEngineAndFindObjectTypeEmptyOrUnknownIdIsNotFound)
{
    m_model->setEngine(engine(kEngineA));

    ASSERT_EQ(m_model->setEngineAndFindObjectType(engine(kEngineB), []() { return QString(); }),
        nullptr);
    ASSERT_EQ(m_filterModel->selectedEngine(), engine(kEngineB));
    ASSERT_EQ(m_model->setEngineAndFindObjectType(
                  engine(kEngineB), []() { return QString("test.object.unknown"); }),
        nullptr);
    ASSERT_EQ(m_model->setEngineAndFindObjectType(nullptr, []() { return QString(); }), nullptr);
    ASSERT_EQ(m_filterModel->selectedEngine(), nullptr);
    ASSERT_TRUE(isCurrentRootObjectType(lookup(kCommonType)));
}

TEST_F(DetectableObjectTypeModelEngineTest, setEngineAndFindObjectTypeFollowsSelectionOfHandler)
{
    m_model->setEngine(engine(kEngineA));

    QString selectedTypeId = kOnlyEngineAType;
    QObject context;
    QObject::connect(m_filterModel.get(),
        &core::analytics::taxonomy::AnalyticsFilterModel::objectTypesChanged,
        &context,
        [&]()
        {
            if (!lookup(selectedTypeId))
                selectedTypeId = kCommonType;
        });

    const auto objectType =
        m_model->setEngineAndFindObjectType(engine(kEngineB), [&]() { return selectedTypeId; });
    ASSERT_EQ(selectedTypeId, kCommonType);
    ASSERT_TRUE(isCurrentRootObjectType(objectType));
    ASSERT_EQ(objectType, lookup(kCommonType));
}

TEST_F(
    DetectableObjectTypeModelEngineTest, setEngineAndFindObjectTypeReenteredFromSwitchNotification)
{
    m_model->setEngine(engine(kEngineA));

    // Repeats the call and clears a type it does not find, as the right panel type button does.
    QString selectedTypeId = kOnlyEngineAType;
    int notificationCount = 0;
    int nestedRequestCount = 0;
    int outerRequestCount = 0;
    bool nestedCallFoundType = true;
    QObject context;
    QObject::connect(m_filterModel.get(),
        &core::analytics::taxonomy::AnalyticsFilterModel::objectTypesChanged,
        &context,
        [&]()
        {
            ++notificationCount;
            const auto nestedObjectType = m_model->setEngineAndFindObjectType(engine(kEngineB),
                [&]()
                {
                    ++nestedRequestCount;
                    return selectedTypeId;
                });
            nestedCallFoundType = nestedObjectType != nullptr;
            if (!nestedObjectType)
                selectedTypeId.clear();
        });

    const auto objectType = m_model->setEngineAndFindObjectType(engine(kEngineB),
        [&]()
        {
            ++outerRequestCount;
            return selectedTypeId;
        });

    ASSERT_EQ(notificationCount, 1);
    ASSERT_EQ(nestedRequestCount, 1);
    ASSERT_EQ(outerRequestCount, 1);
    ASSERT_FALSE(nestedCallFoundType);
    ASSERT_TRUE(selectedTypeId.isEmpty());
    ASSERT_EQ(objectType, nullptr);
    ASSERT_EQ(m_filterModel->selectedEngine(), engine(kEngineB));
}

TEST_F(DetectableObjectTypeModelEngineTest, setEngineAndFindObjectTypeWithoutTaxonomyFindsNothing)
{
    core::analytics::taxonomy::AnalyticsFilterModel filterModel;
    DetectableObjectTypeModel model(&filterModel);

    int requestCount = 0;
    ASSERT_EQ(model.setEngineAndFindObjectType(engine(kEngineA),
                  [&]()
                  {
                      ++requestCount;
                      return kCommonType;
                  }),
        nullptr);
    ASSERT_EQ(requestCount, 1);
    ASSERT_EQ(filterModel.selectedEngine(), nullptr);
    ASSERT_EQ(model.rowCount(QModelIndex()), 0);
}

} // namespace test
} // namespace nx::vms::client::desktop::analytics
