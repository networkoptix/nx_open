// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QFile>

#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/vms/common/test_support/analytics/taxonomy/test_resource_support_proxy.h>
#include <nx/vms/common/test_support/analytics/taxonomy/utils.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

struct ColorTypeTestExpectedData
{
    QString id;
    QString name;
    QString base;
    std::vector<ColorItem> items;
    std::vector<ColorItem> ownItems;
};
#define EnumTypeTestExpectedData_Fields \
    (id) \
    (name) \
    (base) \
    (items) \
    (ownItems)

NX_REFLECTION_INSTRUMENT(ColorTypeTestExpectedData, EnumTypeTestExpectedData_Fields);

class ColorTypeTest: public ::testing::Test
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
            nx::reflect::json::deserialize<std::map<QString, ColorTypeTestExpectedData>>(objectAsBytes.toStdString());

        m_expectedData = deserializationResult;

        ASSERT_TRUE(result.success);
    }

    void afterDescriptorsCompilation()
    {
        m_result = StateCompiler::compile(
            m_descriptors,
            std::make_unique<TestResourceSupportProxy>());
    }

    void makeSureColorTypesAreCorrect()
    {
        const std::vector<AbstractColorType*> colorTypes = m_result.state->colorTypes();
        makeSureOnlyCorrectEnumTypesArePresent(colorTypes);
        for (const AbstractColorType* colorType: colorTypes)
        {
            makeSureNameIsCorrect(colorType);
            makeSureBaseIsCorrect(colorType);
            makeSureOwnItemsAreCorrect(colorType);
            makeSureItemsAreCorrect(colorType);
        }
    }

private:
    void makeSureOnlyCorrectEnumTypesArePresent(const std::vector<AbstractColorType*>& colorTypes)
    {
        for (const AbstractColorType* colorType: colorTypes)
        {
            ASSERT_TRUE(m_expectedData.find(colorType->id()) != m_expectedData.cend());
        }
    }

    void makeSureNameIsCorrect(const AbstractColorType* colorType)
    {
        const ColorTypeTestExpectedData& expectedData = m_expectedData[colorType->id()];
        ASSERT_EQ(colorType->name(), expectedData.name);
    }

    void makeSureBaseIsCorrect(const AbstractColorType* colorType)
    {
        const ColorTypeTestExpectedData& expectedData = m_expectedData[colorType->id()];
        if (expectedData.base.isEmpty())
            ASSERT_EQ(colorType->base(), nullptr);
        else
            ASSERT_EQ(colorType->base()->id(), expectedData.base);
    }

    void makeSureOwnItemsAreCorrect(const AbstractColorType* colorType)
    {
        verifyColorItems(
            colorType,
            m_expectedData[colorType->id()].ownItems,
            colorType->ownItems());
    }

    void makeSureItemsAreCorrect(const AbstractColorType* colorType)
    {
        verifyColorItems(colorType, m_expectedData[colorType->id()].items, colorType->items());
    }

private:
    void verifyColorItems(
        const AbstractColorType* colorType,
        const std::vector<ColorItem>& expectedColorItems,
        const std::vector<QString>& colorItems)
    {
        std::map<QString, QString> colorCodeByName;
        for (const QString& colorName: colorItems)
            colorCodeByName[colorName] = colorType->color(colorName);

        ASSERT_EQ(colorCodeByName.size(), expectedColorItems.size());
        for (const ColorItem& colorItem: expectedColorItems)
        {
            ASSERT_TRUE(colorCodeByName.find(colorItem.name) != colorCodeByName.cend());
            ASSERT_EQ(colorCodeByName[colorItem.name], colorItem.rgb);
        }
    }

private:
    Descriptors m_descriptors;
    std::map<QString, ColorTypeTestExpectedData> m_expectedData;
    StateCompiler::Result m_result;
};

TEST_F(ColorTypeTest, colorTypesAreCorrect)
{
    givenDescriptors(":/content/taxonomy/color_type_test.json");
    afterDescriptorsCompilation();
    makeSureColorTypesAreCorrect();
}

} // namespace nx::analytics::taxonomy
