// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <set>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/client/core/analytics/taxonomy/attribute.h>

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
        std::map<nx::Uuid /*engineId*/, std::set<nx::Uuid /*deviceId*/>>
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

struct TestData
{
    nx::vms::api::analytics::Descriptors descriptors;
    std::map<QString, TestCase> testCases;
};
#define TestData_Fields (descriptors)(testCases)
NX_REFLECTION_INSTRUMENT(TestData, TestData_Fields);

} // namespace nx::vms::client::desktop::analytics::taxonomy
