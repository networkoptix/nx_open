// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QFile>

#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/vms/common/test_support/analytics/taxonomy/test_resource_support_proxy.h>
#include <nx/vms/common/test_support/analytics/taxonomy/utils.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

struct EnumTypeTestExpectedData
{
    QString id;
    QString name;
    QString base;
    std::vector<QString> items;
    std::vector<QString> ownItems;
};
#define EnumTypeTestExpectedData_Fields \
    (id) \
    (name) \
    (base) \
    (items) \
    (ownItems)

NX_REFLECTION_INSTRUMENT(EnumTypeTestExpectedData, EnumTypeTestExpectedData_Fields);

class EnumTypeTest: public ::testing::Test
{
protected:
    void givenDescriptors(const QString& filePath)
    {
        TestData testData;
        ASSERT_TRUE(loadDescriptorsTestData(filePath, &testData));
        m_descriptors = std::move(testData.descriptors);

        const QJsonObject object = testData.fullData["result"].toObject();
        const QByteArray objectAsBytes = QJsonDocument(object).toJson();

        auto [deserializationResult, result] =
            nx::reflect::json::deserialize<std::map<QString, EnumTypeTestExpectedData>>(objectAsBytes.toStdString());

        m_expectedData = deserializationResult;

        ASSERT_TRUE(result.success);
    }

    void afterDescriptorsCompilation()
    {
        m_result = StateCompiler::compile(
            m_descriptors,
            std::make_unique<TestResourceSupportProxy>());
    }

    void makeSureEnumTypesAreCorrect()
    {
        const std::vector<AbstractEnumType*> enumTypes = m_result.state->enumTypes();
        makeSureOnlyCorrectEnumTypesArePresent(enumTypes);
        for (const AbstractEnumType* enumType: enumTypes)
        {
            makeSureNameIsCorrect(enumType);
            makeSureBaseIsCorrect(enumType);
            makeSureOwnItemsAreCorrect(enumType);
            makeSureItemsAreCorrect(enumType);
        }
    }

private:
    void makeSureOnlyCorrectEnumTypesArePresent(const std::vector<AbstractEnumType*>& enumTypes)
    {
        for (const AbstractEnumType* enumType: enumTypes)
        {
            ASSERT_TRUE(m_expectedData.find(enumType->id()) != m_expectedData.cend());
        }
    }

    void makeSureNameIsCorrect(const AbstractEnumType* enumType)
    {
        const EnumTypeTestExpectedData& expectedData = m_expectedData[enumType->id()];
        ASSERT_EQ(enumType->name(), expectedData.name);
    }

    void makeSureBaseIsCorrect(const AbstractEnumType* enumType)
    {
        const EnumTypeTestExpectedData& expectedData = m_expectedData[enumType->id()];
        if (expectedData.base.isEmpty())
            ASSERT_EQ(enumType->base(), nullptr);
        else
            ASSERT_EQ(enumType->base()->id(), expectedData.base);
    }

    void makeSureOwnItemsAreCorrect(const AbstractEnumType* enumType)
    {
        ASSERT_EQ(enumType->ownItems(), m_expectedData[enumType->id()].ownItems);
    }

    void makeSureItemsAreCorrect(const AbstractEnumType* enumType)
    {
        ASSERT_EQ(enumType->items(), m_expectedData[enumType->id()].items);
    }

private:
    Descriptors m_descriptors;
    std::map<QString, EnumTypeTestExpectedData> m_expectedData;
    StateCompiler::Result m_result;
};

TEST_F(EnumTypeTest, enumTypesAreCorrect)
{
    givenDescriptors(":/content/taxonomy/enum_type_test.json");
    afterDescriptorsCompilation();
    makeSureEnumTypesAreCorrect();
}

} // namespace nx::analytics::taxonomy
