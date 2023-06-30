// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QFile>

#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/json/serializer.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/client/core/analytics/taxonomy/abstract_state_view_filter.h>
#include <nx/vms/client/core/analytics/taxonomy/attribute.h>
#include <nx/vms/client/core/analytics/taxonomy/attribute_condition_state_view_filter.h>
#include <nx/vms/client/core/analytics/taxonomy/attribute_set.h>
#include <nx/vms/client/core/analytics/taxonomy/color_set.h>
#include <nx/vms/client/core/analytics/taxonomy/enumeration.h>
#include <nx/vms/client/core/analytics/taxonomy/object_type.h>
#include <nx/vms/client/core/analytics/taxonomy/state_view.h>
#include <nx/vms/client/core/analytics/taxonomy/state_view_builder.h>
#include <nx/vms/client/desktop/analytics/taxonomy/test_support/test_resource_support_proxy.h>

using nx::analytics::taxonomy::AbstractResourceSupportProxy;

namespace nx::vms::client::desktop::analytics::taxonomy {

using namespace nx::vms::client::core::analytics::taxonomy;

struct ExpectedColorItem
{
    QString name;
    QString rgb;
};
#define ExpectedColorItem_Fields (name)(rgb)
NX_REFLECTION_INSTRUMENT(ExpectedColorItem, ExpectedColorItem_Fields);

struct ExpectedAttribute
{
    QString name;
    Attribute::Type type;
    QString subtype;
    std::optional<float> minValue;
    std::optional<float> maxValue;
    QString unit;
    std::vector<QString> enumeration;
    std::vector<ExpectedColorItem> colorSet;
    std::vector<ExpectedAttribute> attributeSet;
};
#define ExpectedAttribute_Fields \
    (name)\
    (type)\
    (subtype)\
    (minValue)\
    (maxValue)\
    (unit)\
    (enumeration)\
    (colorSet)\
    (attributeSet)
NX_REFLECTION_INSTRUMENT(ExpectedAttribute, ExpectedAttribute_Fields);

struct ExpectedObjectType
{
    QString name;
    QString icon;
    std::set<QString> typeIds;
    std::set<QString> fullSubtreeTypeIds;
    std::vector<ExpectedAttribute> attributes;
    std::vector<ExpectedObjectType> objectTypes;
};
#define ExpectedObjectType_Fields (name)(icon)(typeIds)(fullSubtreeTypeIds)(attributes)(objectTypes)
NX_REFLECTION_INSTRUMENT(ExpectedObjectType, ExpectedObjectType_Fields);

struct ExpectedData
{
    std::vector<ExpectedObjectType> objectTypes;
};
#define ExpectedData_Fields (objectTypes)
NX_REFLECTION_INSTRUMENT(ExpectedData, ExpectedData_Fields);

struct InputData
{
    using AttributeSupportInfo = std::map<
        QString /*attributeName*/,
        std::map<QnUuid /*engineId*/, std::set<QnUuid /*deviceId*/>>
    >;

    std::vector<QString> objectTypes;
    std::map<QString, AttributeSupportInfo> attributeSupportInfo;
    std::optional<std::map<QString, QString>> attributeValues;
};
#define InputData_Fields (objectTypes)(attributeSupportInfo)(attributeValues)
NX_REFLECTION_INSTRUMENT(InputData, InputData_Fields);

struct TestCase
{
    InputData input;
    ExpectedData expected;
};
#define TestCase_Fields (input)(expected)
NX_REFLECTION_INSTRUMENT(TestCase, TestCase_Fields);

struct MultiengineTestCase
{
    InputData input;
    std::map</*engineId*/ QnUuid, ExpectedData> expected;
};
#define MultiengineTestCase_Fields (input)(expected)
NX_REFLECTION_INSTRUMENT(MultiengineTestCase, MultiengineTestCase_Fields);

struct TestData
{
    nx::vms::api::analytics::Descriptors descriptors;
    std::map<QString, TestCase> testCases;
};
#define TestData_Fields (descriptors)(testCases)
NX_REFLECTION_INSTRUMENT(TestData, TestData_Fields);

struct MultiengineTestData
{
    nx::vms::api::analytics::Descriptors descriptors;
    std::map<QString, MultiengineTestCase> testCases;
};
#define MultiengineTestData_Fields (descriptors)(testCases)
NX_REFLECTION_INSTRUMENT(MultiengineTestData, MultiengineTestData_Fields);

class StateViewTest: public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        QFile dataFile(":/state_view_ut/test_data.json");
        ASSERT_TRUE(dataFile.open(QIODevice::ReadOnly));

        s_testData = std::make_unique<TestData>();
        ASSERT_TRUE(nx::reflect::json::deserialize(nx::Buffer(dataFile.readAll()), s_testData.get()));

        QFile multiengineDataFile(":/state_view_ut/multiengine_test_data.json");
        ASSERT_TRUE(multiengineDataFile.open(QIODevice::ReadOnly));

        s_multiengineTestData = std::make_unique<MultiengineTestData>();
        ASSERT_TRUE(nx::reflect::json::deserialize(
            nx::Buffer(multiengineDataFile.readAll()), s_multiengineTestData.get()));
    }

    static void TearDownTestSuite()
    {
        s_testData.reset();
        s_multiengineTestData.reset();
    }

    void givenDescriptors(const QString& testCaseName)
    {
        const auto itr = s_testData->testCases.find(testCaseName);
        ASSERT_FALSE(itr == s_testData->testCases.cend());

        m_testCase = itr->second;
        m_descriptors = prepareDescriptors(m_testCase.input, s_testData->descriptors);
        m_resourceSupportProxy = prepareResourceSupportProxy(
            m_testCase.input.attributeSupportInfo);
        m_attributeValues = m_testCase.input.attributeValues;
    }

    void givenMultiengineDescriptors(const QString& testCaseName)
    {
        const auto itr = s_multiengineTestData->testCases.find(testCaseName);
        ASSERT_FALSE(itr == s_multiengineTestData->testCases.cend());

        m_multiengineTestCase = itr->second;
        m_descriptors = prepareDescriptors(m_multiengineTestCase.input, s_multiengineTestData->descriptors);
        m_resourceSupportProxy = prepareResourceSupportProxy(
            m_multiengineTestCase.input.attributeSupportInfo);
    }

    void afterStateViewIsBuilt()
    {
        const nx::analytics::taxonomy::StateCompiler::Result result =
            nx::analytics::taxonomy::StateCompiler::compile(
                m_descriptors,
                std::move(m_resourceSupportProxy));

        m_stateViewBuilder = std::make_unique<StateViewBuilder>(result.state);
    }

    void makeSureStateViewIsCorrect()
    {
        std::unique_ptr<AbstractStateViewFilter> filter;
        if (m_attributeValues)
        {
            nx::common::metadata::Attributes attributes;
            for (const auto& [attributeName, attributeValue]: *m_attributeValues)
            {
                attributes.push_back(
                    nx::common::metadata::Attribute{attributeName, attributeValue});
            }
            filter = std::make_unique<AttributeConditionStateViewFilter>(
                /*baseFilter*/ nullptr, std::move(attributes));
        }
        const StateView* stateView = m_stateViewBuilder->stateView(filter.get());
        std::vector<ObjectType*> objectTypes = stateView->objectTypes();

        validateObjectTypes(objectTypes, m_testCase.expected.objectTypes);
    }

    void makeSureMultiengineStateViewIsCorrect()
    {
        const std::vector<AbstractStateViewFilter*> engineFilters =
            m_stateViewBuilder->engineFilters();

        // -1 stands for the "all Engines" case.
        ASSERT_EQ(engineFilters.size(), m_multiengineTestCase.expected.size() - 1);

        for (const AbstractStateViewFilter* engineFilter: engineFilters)
        {
            const auto itr = m_multiengineTestCase.expected.find(
                QnUuid::fromStringSafe(engineFilter->id()));
            ASSERT_FALSE(itr == m_multiengineTestCase.expected.cend())
                << engineFilter->id().toStdString();

            const StateView* stateView = m_stateViewBuilder->stateView(engineFilter);
            std::vector<ObjectType*> objectTypes = stateView->objectTypes();

            validateObjectTypes(objectTypes, itr->second.objectTypes);
        }

        const StateView* stateView = m_stateViewBuilder->stateView();
        std::vector<ObjectType*> objectTypes = stateView->objectTypes();

        validateObjectTypes(objectTypes, m_multiengineTestCase.expected[QnUuid()].objectTypes);
    }

private:
    static std::map<QString, std::set<QnUuid>> toDescriptorAttributeSupportInfo(
        const InputData::AttributeSupportInfo& inputDataAttributeSupportInfo)
    {
        std::map<QString, std::set<QnUuid>> result;
        for (const auto& [attributeName, supportInfoByEngineAndDevice]: inputDataAttributeSupportInfo)
        {
            for (const auto& [engineId, deviceIds]: supportInfoByEngineAndDevice)
                result[attributeName].insert(engineId);
        }

        return result;
    }

    static nx::vms::api::analytics::Descriptors prepareDescriptors(
        const InputData& inputData,
        const nx::vms::api::analytics::Descriptors& commonDescriptors)
    {
        nx::vms::api::analytics::Descriptors descriptors = commonDescriptors;
        descriptors.objectTypeDescriptors.clear();
        for (const QString& objectTypeId: inputData.objectTypes)
        {
            const auto itr = commonDescriptors.objectTypeDescriptors.find(objectTypeId);
            EXPECT_FALSE(itr == commonDescriptors.objectTypeDescriptors.cend());

            descriptors.objectTypeDescriptors[objectTypeId] = itr->second;
        }

        for (const auto& [objectTypeId, attributeSupportInfo]: inputData.attributeSupportInfo)
        {
            descriptors.objectTypeDescriptors[objectTypeId].attributeSupportInfo =
                toDescriptorAttributeSupportInfo(attributeSupportInfo);

            if (!attributeSupportInfo.empty())
                descriptors.objectTypeDescriptors[objectTypeId].hasEverBeenSupported = true;
        }

        return descriptors;
    };

    std::unique_ptr<AbstractResourceSupportProxy> prepareResourceSupportProxy(
        const std::map<QString, InputData::AttributeSupportInfo>& attributeSupportInfo)
    {
        return std::make_unique<TestResourceSupportProxy>(attributeSupportInfo);
    }

    void validateObjectTypes(
        const std::vector<ObjectType*>& objectTypes,
        const std::vector<ExpectedObjectType>& expectedObjectTypes)
    {
        ASSERT_EQ(objectTypes.size(), expectedObjectTypes.size());
        for (int i = 0; i < objectTypes.size(); ++i)
        {
            const ObjectType* objectType = objectTypes[i];
            const ExpectedObjectType& expected = expectedObjectTypes[i];

            ASSERT_EQ(objectType->name(), expected.name);
            ASSERT_EQ(objectType->icon(), expected.icon);

            validateTypeIds(objectType, expected);

            const std::vector<Attribute*> attributes = objectType->attributes();
            const std::vector<ExpectedAttribute> expectedAttributes = expected.attributes;

            validateAttributes(attributes, expectedAttributes);
            validateObjectTypes(objectType->derivedObjectTypes(), expected.objectTypes);
        }
    }

    void validateTypeIds(const ObjectType* objectType, const ExpectedObjectType& expected)
    {
        const std::vector<QString> actualTypeIds = objectType->typeIds();
        const std::set<QString> actualTypeIdSet(actualTypeIds.begin(), actualTypeIds.end());

        const std::vector<QString> actualFullSubtreeTypeIds = objectType->fullSubtreeTypeIds();
        const std::set<QString> actualFullSubtreeTypeIdSet(
            actualFullSubtreeTypeIds.begin(), actualFullSubtreeTypeIds.end());

        ASSERT_EQ(actualTypeIdSet, expected.typeIds) << expected.name.toStdString();
        ASSERT_EQ(actualFullSubtreeTypeIdSet, expected.fullSubtreeTypeIds)
            << expected.name.toStdString();
    }

    void validateAttributes(
        const std::vector<Attribute*>& attributes,
        const std::vector<ExpectedAttribute>& expectedAttributes)
    {
        ASSERT_EQ(attributes.size(), expectedAttributes.size());
        for (int i = 0; i < attributes.size(); ++i)
        {
            const Attribute* attribute = attributes[i];
            const ExpectedAttribute& expectedAttribute = expectedAttributes[i];

            ASSERT_EQ(attribute->type, expectedAttribute.type);
            ASSERT_EQ(attribute->name, expectedAttribute.name);
            switch(attribute->type)
            {
                case Attribute::Type::number:
                    validateNumericAttribute(attribute, expectedAttribute);
                    break;
                case Attribute::Type::enumeration:
                    validateEnumerationAttribute(attribute, expectedAttribute);
                    break;
                case Attribute::Type::colorSet:
                    validateColorSetAttribute(attribute, expectedAttribute);
                    break;
                case Attribute::Type::attributeSet:
                    validateAttributeSetAttribute(attribute, expectedAttribute);
                    break;
            }
        }
    }

    void validateNumericAttribute(
        const Attribute* attribute,
        const ExpectedAttribute& expectedAttribute)
    {
        const QString subtype = attribute->subtype;

        ASSERT_EQ(subtype, expectedAttribute.subtype);
        const QVariant minValue = attribute->minValue;
        const QVariant maxValue = attribute->maxValue;

        if (minValue.isNull())
            ASSERT_FALSE(expectedAttribute.minValue.has_value());
        else
            ASSERT_TRUE(expectedAttribute.minValue.has_value());

        validateBounds(subtype, minValue, expectedAttribute.minValue);

        if (maxValue.isNull())
            ASSERT_FALSE(expectedAttribute.maxValue.has_value());
        else
            ASSERT_TRUE(expectedAttribute.maxValue.has_value());

        validateBounds(subtype, maxValue, expectedAttribute.maxValue);

        ASSERT_EQ(attribute->unit, expectedAttribute.unit);
    }

    void validateBounds(
        const QString& subtype,
        const QVariant& value,
        const std::optional<float>& expected)
    {
        if (value.isNull())
            return;

        if (subtype == nx::analytics::taxonomy::kIntegerAttributeSubtype)
            ASSERT_EQ(value.toInt(), (int) expected.value());
        else if (subtype == nx::analytics::taxonomy::kFloatAttributeSubtype)
            ASSERT_FLOAT_EQ(value.toFloat(), expected.value());
        else
            FAIL();
    }

    void validateEnumerationAttribute(
        const Attribute* attribute,
        const ExpectedAttribute& expectedAttribute)
    {
        const Enumeration* enumeration = attribute->enumeration;
        ASSERT_TRUE(enumeration);

        const std::vector<QString> items = enumeration->items();
        ASSERT_EQ(items.size(), expectedAttribute.enumeration.size());

        for (int i = 0; i < items.size(); ++i)
            ASSERT_EQ(items[i], expectedAttribute.enumeration[i]);
    }

    void validateColorSetAttribute(
        const Attribute* attribute,
        const ExpectedAttribute& expectedAttribute)
    {
        const ColorSet* colorSet = attribute->colorSet;
        ASSERT_TRUE(colorSet);

        const std::vector<QString> items = colorSet->items();
        ASSERT_EQ(items.size(), expectedAttribute.colorSet.size());

        for (int i = 0; i < items.size(); ++i)
        {
            ASSERT_EQ(items[i], expectedAttribute.colorSet[i].name);
            ASSERT_EQ(colorSet->color(items[i]), expectedAttribute.colorSet[i].rgb);
        }
    }

    void validateAttributeSetAttribute(
        const Attribute* attribute,
        const ExpectedAttribute& expectedAttribute)
    {
        const AttributeSet* attributeSet = attribute->attributeSet;
        ASSERT_TRUE(attributeSet);

        const std::vector<Attribute*> attributes = attributeSet->attributes();
        const std::vector<ExpectedAttribute> expectedAttributes = expectedAttribute.attributeSet;

        validateAttributes(attributes, expectedAttributes);
    }


private:
    static inline std::unique_ptr<TestData> s_testData = nullptr;
    static inline std::unique_ptr<MultiengineTestData> s_multiengineTestData = nullptr;
    TestCase m_testCase;
    MultiengineTestCase m_multiengineTestCase;
    nx::vms::api::analytics::Descriptors m_descriptors;
    std::optional<std::map<QString, QString>> m_attributeValues;
    std::unique_ptr<StateViewBuilder> m_stateViewBuilder;
    std::unique_ptr<AbstractResourceSupportProxy> m_resourceSupportProxy;
};

TEST_F(StateViewTest, singleHiddenType_noClashingAttributes)
{
    givenDescriptors("singleHiddenType_noClashingAttributes");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, singleHiddenType_clashingNumericAttribute)
{
    givenDescriptors("singleHiddenType_clashingNumericAttribute");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, singleHiddenType_clashingEnumerationAttribute)
{
    givenDescriptors("singleHiddenType_clashingEnumerationAttribute");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, singleHiddenType_clashingColorSetAttribute)
{
    givenDescriptors("singleHiddenType_clashingColorSetAttribute");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, singleHiddenType_clashingAttributeSetAttribute)
{
    givenDescriptors("singleHiddenType_clashingAttributeSetAttribute");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, singleHiddenType_clashingBooleanAttribute)
{
    givenDescriptors("singleHiddenType_clashingBooleanAttribute");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, singleHiddenType_clashingStringAttribute)
{
    givenDescriptors("singleHiddenType_clashingStringAttribute");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

//-------------------------------------------------------------------------------------------------

TEST_F(StateViewTest, multipleHiddenTypes_noClashingAttributes)
{
    givenDescriptors("multipleHiddenTypes_noClashingAttributes");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, multipleHiddenTypes_clashingNumericAttribute)
{
    givenDescriptors("multipleHiddenTypes_clashingNumericAttribute");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, multipleHiddenTypes_clashingEnumerationAttribute)
{
    givenDescriptors("multipleHiddenTypes_clashingEnumerationAttribute");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, multipleHiddenTypes_clashingColorSetAttribute)
{
    givenDescriptors("multipleHiddenTypes_clashingColorSetAttribute");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, multipleHiddenTypes_clashingAttributeSetAttribute)
{
    givenDescriptors("multipleHiddenTypes_clashingAttributeSetAttribute");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, multipleHiddenTypes_clashingBooleanAttribute)
{
    givenDescriptors("multipleHiddenTypes_clashingBooleanAttribute");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, multipleHiddenTypes_clashingStringAttribute)
{
    givenDescriptors("multipleHiddenTypes_clashingStringAttribute");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, conditionalAttributes_1)
{
    givenDescriptors("conditionalAttributes_1");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, conditionalAttributes_2)
{
    givenDescriptors("conditionalAttributes_2");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, conditionalAttributes_3)
{
    givenDescriptors("conditionalAttributes_3");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, conditionalAttributes_4)
{
    givenDescriptors("conditionalAttributes_4");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

//-------------------------------------------------------------------------------------------------

TEST_F(StateViewTest, derivedTypes)
{
    givenDescriptors("derivedTypes");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, derivedTypesWithHiddenDescendants)
{
    givenDescriptors("derivedTypesWithHiddenDescendants");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

TEST_F(StateViewTest, derivedTypesWithHiddenDescendants_onlyHiddenTypesAreSupported)
{
    givenDescriptors("derivedTypesWithHiddenDescendants_onlyHiddenTypesAreSupported");
    afterStateViewIsBuilt();
    makeSureStateViewIsCorrect();
}

//-------------------------------------------------------------------------------------------------

TEST_F(StateViewTest, multiengineTestCase_withoutHiddenTypes)
{
    givenMultiengineDescriptors("multiengineTestCase_withoutHiddenTypes");
    afterStateViewIsBuilt();
    makeSureMultiengineStateViewIsCorrect();
}

TEST_F(StateViewTest, multiengineTestCase_withHiddenTypes)
{
    givenMultiengineDescriptors("multiengineTestCase_withHiddenTypes");
    afterStateViewIsBuilt();
    makeSureMultiengineStateViewIsCorrect();
}

} // namespace nx::vms::client::desktop::analytics::taxonomy
