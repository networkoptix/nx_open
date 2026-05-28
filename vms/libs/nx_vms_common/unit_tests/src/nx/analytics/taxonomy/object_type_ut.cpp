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
    std::string id;
    std::string name;
    std::string base;
    std::set<std::string> derivedTypes;
    std::set<std::string> directDerivedTypes;
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
    std::string engineId;
    std::string engineName;
    std::string groupId;
    std::string groupName;
    std::string integrationId;
    std::string integrationName;
};
#define ScopeInfo_Fields \
    (engineId) \
    (engineName) \
    (groupId) \
    (groupName) \
    (integrationId) \
    (integrationName)

bool operator<(const ScopeInfo& lhs, const ScopeInfo& rhs)
{
    if (lhs.engineId != rhs.engineId)
        return lhs.engineId < rhs.engineId;

    return lhs.groupId < rhs.groupId;
}

NX_REFLECTION_INSTRUMENT(ScopeInfo, ScopeInfo_Fields);

struct ScopeTestExpectedData
{
    std::string id;
    std::string name;
    std::set<ScopeInfo> scopeInfo;
};
#define ScopeTestExpectedData_Fields \
    (id) \
    (name) \
    (scopeInfo)

NX_REFLECTION_INSTRUMENT(ScopeTestExpectedData, ScopeTestExpectedData_Fields);

struct FlagTestExpectedData
{
    std::string id;
    std::string name;
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
    void givenDescriptors(const std::string& filePath)
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
        std::map<std::string, std::set<std::string>> supportedAttributesByTypeId;
        for (const auto& [descriptorId, descriptor] : m_testData.descriptors.objectTypeDescriptors)
        {
            for (const auto& [attributeName, _] : descriptor.attributeSupportInfo)
                supportedAttributesByTypeId[descriptorId].insert(attributeName);
        }

        for (const ObjectType* objectType : m_result.state->objectTypes())
            verifySupportedAttributesAreCorrect(objectType, supportedAttributesByTypeId);
    }

    void makeSureReachableTypesAreCorrect()
    {
        std::set<std::string> expectedResult;

        const QJsonArray array = m_testData.fullData["nonReachableTypes"].toArray();
        const QByteArray arrayAsBytes = QJsonDocument(array).toJson();

        auto [deserializationResult, result] =
            nx::reflect::json::deserialize<std::set<std::string>>(arrayAsBytes.toStdString());

        expectedResult = deserializationResult;

        ASSERT_TRUE(result.success);

        ASSERT_EQ(nonReachableTypeIds(m_result.state), expectedResult);
    }

    void makeSureInheritanceIsCorrect()
    {
        std::map<std::string, InheritanceTestExpectedData> expectedData;

        const QJsonObject object = m_testData.fullData["result"].toObject();
        const QByteArray obectAsBytes = QJsonDocument(object).toJson();

        auto [deserializationResult, result] =
            nx::reflect::json::deserialize<std::map<std::string, InheritanceTestExpectedData>>(obectAsBytes.toStdString());

        expectedData = deserializationResult;

        ASSERT_TRUE(result.success);

        ASSERT_FALSE(expectedData.empty());

        const std::vector<ObjectType*> objectTypes = m_result.state->objectTypes();

        ASSERT_EQ(objectTypes.size(), expectedData.size());
        for (const ObjectType* objectType: objectTypes)
        {
            const InheritanceTestExpectedData& expected = expectedData[objectType->id()];
            ASSERT_EQ(expected.id, objectType->id());
            ASSERT_EQ(expected.name, objectType->name());

            const ObjectType* base = objectType->base();
            if (!expected.base.empty())
                ASSERT_EQ(expected.base, base->id());
            else
                ASSERT_EQ(base, nullptr);

            const std::vector<ObjectType*> directDerivedTypes = objectType->derivedTypes();
            std::set<std::string> directDerivedTypeIds;
            for (const ObjectType* directDerivedType:  directDerivedTypes)
                directDerivedTypeIds.insert(directDerivedType->id());

            ASSERT_EQ(directDerivedTypeIds, expected.directDerivedTypes);

            std::set<std::string> allDerivedTypeIds =
                getAllDerivedTypeIds(m_result.state.get(), objectType->id());

            ASSERT_EQ(allDerivedTypeIds, expected.derivedTypes);
        }
    }

    void makeSureScopesAreCorrect()
    {
        std::map<std::string, ScopeTestExpectedData> expectedData;

        const QJsonObject object = m_testData.fullData["result"].toObject();
        const QByteArray obectAsBytes = QJsonDocument(object).toJson();

        auto [deserializationResult, result] =
            nx::reflect::json::deserialize<std::map<std::string, ScopeTestExpectedData>>(obectAsBytes.toStdString());

        expectedData = deserializationResult;

        ASSERT_TRUE(result.success);

        ASSERT_FALSE(expectedData.empty());

        const std::vector<ObjectType*> objectTypes = m_result.state->objectTypes();

        ASSERT_EQ(objectTypes.size(), expectedData.size());
        for (const ObjectType* objectType: objectTypes)
        {
            const ScopeTestExpectedData& expected = expectedData[objectType->id()];
            ASSERT_EQ(expected.id, objectType->id());
            ASSERT_EQ(expected.name, objectType->name());

            const auto& scopes = objectType->scopes();
            ASSERT_EQ(scopes.size(), expected.scopeInfo.size());
            for (const auto& scope: scopes)
            {
                ScopeInfo scopeInfo;
                const AbstractEngine* engine = scope.engine();
                ASSERT_TRUE(engine);

                const AbstractGroup* group = scope.group();

                AbstractIntegration* integration = nullptr;
                if (engine)
                {
                    scopeInfo.engineId = engine->id();
                    integration = engine->integration();
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

                if (integration)
                {
                    ASSERT_EQ(scopeItr->integrationId, integration->id());
                    ASSERT_EQ(scopeItr->integrationName, integration->name());
                }
            }
        }
    }

    void makeSureFlagsAreCorrect()
    {
        std::map<std::string, FlagTestExpectedData> expectedData;

        const QJsonObject object = m_testData.fullData["result"].toObject();
        const QByteArray obectAsBytes = QJsonDocument(object).toJson();

        auto [deserializationResult, result] =
            nx::reflect::json::deserialize<std::map<std::string, FlagTestExpectedData>>(obectAsBytes.toStdString());

        expectedData = deserializationResult;

        ASSERT_TRUE(result.success);

        ASSERT_FALSE(expectedData.empty());

        const std::vector<ObjectType*> objectTypes = m_result.state->objectTypes();

        ASSERT_EQ(objectTypes.size(), expectedData.size());
        for (const ObjectType* objectType: objectTypes)
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
        const ObjectType* objectType,
        const std::map<std::string, std::set<std::string>> supportedAttributesByTypeId)
    {
        const auto it = supportedAttributesByTypeId.find(objectType->id());
        ASSERT_EQ(
            it == supportedAttributesByTypeId.cend() ? std::set<std::string>() : it->second,
            allAttributePaths(objectType));
    }

    std::set<std::string> allAttributePaths(
        const ObjectType* objectType,
        const std::string& prefix = std::string())
    {
        std::set<std::string> result;
        for (const AbstractAttribute* attribute : objectType->supportedAttributes())
        {
            result.insert(prefix + attribute->name());

            if (attribute->type() != AbstractAttribute::Type::object)
                continue;

            std::set<std::string> nestedAttributes = allAttributePaths(
                attribute->objectType(), prefix + attribute->name() + ".");

            result.insert(nestedAttributes.begin(), nestedAttributes.end());
        }

        return result;
    }

    std::set<std::string> nonReachableTypeIds(const std::shared_ptr<AbstractState>& state)
    {
        std::set<std::string> result;
        for (const ObjectType* objectType : state->objectTypes())
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
