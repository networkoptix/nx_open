// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QFile>

#include <nx/analytics/taxonomy/helpers.h>
#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/vms/common/test_support/analytics/taxonomy/test_resource_support_proxy.h>
#include <nx/vms/common/test_support/analytics/taxonomy/utils.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

struct InheritanceTestExpectedData
{
    QString id;
    QString name;
    QString base;
    std::set<QString> derivedTypes;
    std::set<QString> directDerivedTypes;
};
#define InheritanceTestExpectedData_Fields \
    (id) \
    (name) \
    (base) \
    (derivedTypes) \
    (directDerivedTypes)

NX_REFLECTION_INSTRUMENT(InheritanceTestExpectedData, InheritanceTestExpectedData_Fields);

struct ScopeInfo
{
    QString engineId;
    QString engineName;
    QString groupId;
    QString groupName;
    QString pluginId;
    QString pluginName;
};
#define ScopeInfo_Fields \
    (engineId) \
    (engineName) \
    (groupId) \
    (groupName) \
    (pluginId) \
    (pluginName)

bool operator<(const ScopeInfo& lhs, const ScopeInfo& rhs)
{
    if (lhs.engineId != rhs.engineId)
        return lhs.engineId < rhs.engineId;

    return lhs.groupId < rhs.groupId;
}

NX_REFLECTION_INSTRUMENT(ScopeInfo, ScopeInfo_Fields);

struct ScopeTestExpectedData
{
    QString id;
    QString name;
    std::set<ScopeInfo> scopeInfo;
};
#define ScopeTestExpectedData_Fields \
    (id) \
    (name) \
    (scopeInfo)

NX_REFLECTION_INSTRUMENT(ScopeTestExpectedData, ScopeTestExpectedData_Fields);

struct FlagTestExpectedData
{
    QString id;
    QString name;
    bool liveOnly = false;
    bool nonIndexable = false;
};
#define FlagTestExpectedData_Fields \
    (id) \
    (name) \
    (liveOnly) \
    (nonIndexable)

NX_REFLECTION_INSTRUMENT(FlagTestExpectedData, FlagTestExpectedData_Fields);

class ObjectTypeTest: public ::testing::Test
{
protected:
    void givenDescriptors(const QString& filePath)
    {
        ASSERT_TRUE(loadDescriptorsTestData(filePath, &m_testData));
    }

    void afterDescriptorsCompilation()
    {
        m_result = StateCompiler::compile(
            m_testData.descriptors,
            std::make_unique<TestResourceSupportProxy>());
    }

    void makeSureSupportedAttributesAreCorrect()
    {
        std::map<QString, std::set<QString>> supportedAttributesByTypeId;
        for (const auto& [descriptorId, descriptor] : m_testData.descriptors.objectTypeDescriptors)
        {
            for (const auto& [attributeName, _] : descriptor.attributeSupportInfo)
                supportedAttributesByTypeId[descriptorId].insert(attributeName);
        }

        for (const AbstractObjectType* objectType : m_result.state->objectTypes())
            verifySupportedAttributesAreCorrect(objectType, supportedAttributesByTypeId);
    }

    void makeSureReachableTypesAreCorrect()
    {
        std::set<QString> expectedResult;

        const QJsonArray array = m_testData.fullData["nonReachableTypes"].toArray();
        const QByteArray arrayAsBytes = QJsonDocument(array).toJson();

        auto [deserializationResult, result] =
            nx::reflect::json::deserialize<std::set<QString>>(arrayAsBytes.toStdString());

        expectedResult = deserializationResult;

        ASSERT_TRUE(result.success);

        ASSERT_EQ(nonReachableTypeIds(m_result.state), expectedResult);
    }

    void makeSureInheritanceIsCorrect()
    {
        std::map<QString, InheritanceTestExpectedData> expectedData;

        const QJsonObject object = m_testData.fullData["result"].toObject();
        const QByteArray obectAsBytes = QJsonDocument(object).toJson();

        auto [deserializationResult, result] =
            nx::reflect::json::deserialize<std::map<QString, InheritanceTestExpectedData>>(obectAsBytes.toStdString());

        expectedData = deserializationResult;

        ASSERT_TRUE(result.success);

        ASSERT_FALSE(expectedData.empty());

        const std::vector<AbstractObjectType*> objectTypes = m_result.state->objectTypes();

        ASSERT_EQ(objectTypes.size(), expectedData.size());
        for (const AbstractObjectType* objectType: objectTypes)
        {
            const InheritanceTestExpectedData& expected = expectedData[objectType->id()];
            ASSERT_EQ(expected.id, objectType->id());
            ASSERT_EQ(expected.name, objectType->name());

            const AbstractObjectType* base = objectType->base();
            if (!expected.base.isEmpty())
                ASSERT_EQ(expected.base, base->id());
            else
                ASSERT_EQ(base, nullptr);

            const std::vector<AbstractObjectType*> directDerivedTypes = objectType->derivedTypes();
            std::set<QString> directDerivedTypeIds;
            for (const AbstractObjectType* directDerivedType:  directDerivedTypes)
                directDerivedTypeIds.insert(directDerivedType->id());

            ASSERT_EQ(directDerivedTypeIds, expected.directDerivedTypes);

            std::set<QString> allDerivedTypeIds =
                getAllDerivedTypeIds(m_result.state.get(), objectType->id());

            ASSERT_EQ(allDerivedTypeIds, expected.derivedTypes);
        }
    }

    void makeSureScopesAreCorrect()
    {
        std::map<QString, ScopeTestExpectedData> expectedData;

        const QJsonObject object = m_testData.fullData["result"].toObject();
        const QByteArray obectAsBytes = QJsonDocument(object).toJson();

        auto [deserializationResult, result] =
            nx::reflect::json::deserialize<std::map<QString, ScopeTestExpectedData>>(obectAsBytes.toStdString());

        expectedData = deserializationResult;

        ASSERT_TRUE(result.success);

        ASSERT_FALSE(expectedData.empty());

        const std::vector<AbstractObjectType*> objectTypes = m_result.state->objectTypes();

        ASSERT_EQ(objectTypes.size(), expectedData.size());
        for (const AbstractObjectType* objectType: objectTypes)
        {
            const ScopeTestExpectedData& expected = expectedData[objectType->id()];
            ASSERT_EQ(expected.id, objectType->id());
            ASSERT_EQ(expected.name, objectType->name());

            const std::vector<AbstractScope*> scopes = objectType->scopes();
            ASSERT_EQ(scopes.size(), expected.scopeInfo.size());
            for (const AbstractScope* scope: scopes)
            {
                ScopeInfo scopeInfo;
                const AbstractEngine* engine = scope->engine();
                ASSERT_TRUE(engine);

                const AbstractGroup* group = scope->group();

                AbstractPlugin* plugin = nullptr;
                if (engine)
                {
                    scopeInfo.engineId = engine->id();
                    plugin = engine->plugin();
                }

                if (group)
                    scopeInfo.groupId = group->id();

                const auto scopeItr = expected.scopeInfo.find(scopeInfo);
                ASSERT_TRUE(scopeItr != expected.scopeInfo.cend());

                if (engine)
                {
                    ASSERT_EQ(scopeItr->engineId, engine->id());
                    ASSERT_EQ(scopeItr->engineName, engine->name());
                }

                if (group)
                {
                    ASSERT_EQ(scopeItr->groupId, group->id());
                    ASSERT_EQ(scopeItr->groupName, group->name());
                }

                if (plugin)
                {
                    ASSERT_EQ(scopeItr->pluginId, plugin->id());
                    ASSERT_EQ(scopeItr->pluginName, plugin->name());
                }
            }
        }
    }

    void makeSureFlagsAreCorrect()
    {
        std::map<QString, FlagTestExpectedData> expectedData;

        const QJsonObject object = m_testData.fullData["result"].toObject();
        const QByteArray obectAsBytes = QJsonDocument(object).toJson();

        auto [deserializationResult, result] =
            nx::reflect::json::deserialize<std::map<QString, FlagTestExpectedData>>(obectAsBytes.toStdString());

        expectedData = deserializationResult;

        ASSERT_TRUE(result.success);

        ASSERT_FALSE(expectedData.empty());

        const std::vector<AbstractObjectType*> objectTypes = m_result.state->objectTypes();

        ASSERT_EQ(objectTypes.size(), expectedData.size());
        for (const AbstractObjectType* objectType: objectTypes)
        {
            const FlagTestExpectedData& expected = expectedData[objectType->id()];
            ASSERT_EQ(objectType->id(), expected.id);
            ASSERT_EQ(objectType->name(), expected.name);
            ASSERT_EQ(objectType->isLiveOnly(), expected.liveOnly);
            ASSERT_EQ(objectType->isNonIndexable(), expected.nonIndexable);
        }
    }

private:
    void verifySupportedAttributesAreCorrect(
        const AbstractObjectType* objectType,
        const std::map<QString, std::set<QString>> supportedAttributesByTypeId)
    {
        const auto it = supportedAttributesByTypeId.find(objectType->id());
        ASSERT_EQ(
            it == supportedAttributesByTypeId.cend() ? std::set<QString>() : it->second,
            allAttributePaths(objectType));
    }

    std::set<QString> allAttributePaths(
        const AbstractObjectType* objectType,
        const QString& prefix = QString())
    {
        std::set<QString> result;
        for (const AbstractAttribute* attribute : objectType->supportedAttributes())
        {
            result.insert(prefix + attribute->name());

            if (attribute->type() != AbstractAttribute::Type::object)
                continue;

            std::set<QString> nestedAttributes = allAttributePaths(
                attribute->objectType(), prefix + attribute->name() + ".");

            result.insert(nestedAttributes.begin(), nestedAttributes.end());
        }

        return result;
    }

    std::set<QString> nonReachableTypeIds(const std::shared_ptr<AbstractState>& state)
    {
        std::set<QString> result;
        for (const AbstractObjectType* objectType : state->objectTypes())
        {
            if (!objectType->isReachable())
                result.insert(objectType->id());
        }

        return result;
    }

private:
    TestData m_testData;
    StateCompiler::Result m_result;
};

TEST_F(ObjectTypeTest, supportedAttributes)
{
    givenDescriptors(":/content/taxonomy/supported_object_type_attributes_test.json");
    afterDescriptorsCompilation();
    makeSureSupportedAttributesAreCorrect();
}

TEST_F(ObjectTypeTest, reachability)
{
    givenDescriptors(":/content/taxonomy/object_type_reachability_test.json");
    afterDescriptorsCompilation();
    makeSureReachableTypesAreCorrect();
}

TEST_F(ObjectTypeTest, inheritance)
{
    givenDescriptors(":/content/taxonomy/object_type_inheritance_test.json");
    afterDescriptorsCompilation();
    makeSureInheritanceIsCorrect();
}

TEST_F(ObjectTypeTest, scopes)
{
    givenDescriptors(":/content/taxonomy/object_type_scopes_test.json");
    afterDescriptorsCompilation();
    makeSureScopesAreCorrect();
}

TEST_F(ObjectTypeTest, flags)
{
    givenDescriptors(":/content/taxonomy/object_type_flags_test.json");
    afterDescriptorsCompilation();
    makeSureFlagsAreCorrect();
}

} // namespace nx::analytics::taxonomy
