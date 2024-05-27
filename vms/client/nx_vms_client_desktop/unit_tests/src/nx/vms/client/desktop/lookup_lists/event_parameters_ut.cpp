// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/rules/model_view/event_parameters_model.h>
#include <nx/vms/client/desktop/rules/utils/separator.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::rules::test {

using namespace nx::vms::client::desktop::rules;
using namespace nx::vms::rules::utils;
using namespace nx::vms::rules;

const EventParameterHelper::EventParametersNames kEvenParameterList = {
    "group0.parameter",
    "event.group1", "event.group1.sub_arg", "event.group1.sub_arg2", "event.value",
    "group2.parameter"};

void thenNumberOfGroupsIs(const EventParametersModel& model, int numberOfGroups)
{
    int separatorNumber = 0;
    for (int r = 0; r < model.rowCount({}); ++r)
    {
        const auto dataByaccessRole = model.data(model.index(r), Qt::AccessibleDescriptionRole);
        if (dataByaccessRole.canConvert<Separator>())
            separatorNumber += 1;
    }

    ASSERT_EQ(separatorNumber, numberOfGroups);
}

void thenRowCountIs(const EventParametersModel& model, int numberOfRows)
{
    ASSERT_EQ(model.rowCount({}), numberOfRows);
}

void thenModelIsInDefaultState(const EventParametersModel& model)
{
    ASSERT_EQ(model.rowCount({}), 6);

    int separatorNumber = 0;
    for (int r = 0; r < model.rowCount({}); ++r)
    {
        const auto el = model.data(model.index(r), Qt::DisplayRole);
        ASSERT_NE(el.toString(), "event.group1.sub_arg");
        ASSERT_NE(el.toString(), "event.group1.sub_arg2");
        auto accessRole = model.data(model.index(r), Qt::AccessibleDescriptionRole);
        if (accessRole.canConvert<Separator>())
            separatorNumber += 1;
    }

    ASSERT_EQ(separatorNumber, 2);
}

void thenNumberOfRowsEqualToExpanded(const EventParametersModel& model)
{
    thenRowCountIs(model, 8);
}

void thenIsExpanded(const EventParametersModel& model)
{
    thenNumberOfRowsEqualToExpanded(model);

    EventParameterHelper::EventParametersNames visibleNames;
    int separatorNumber = 0;
    for (int r = 0; r < model.rowCount({}); ++r)
    {
        const auto dataByaccessRole = model.data(model.index(r), Qt::AccessibleDescriptionRole);
        if (dataByaccessRole.canConvert<Separator>())
        {
            separatorNumber += 1;
            continue;
        }
        const auto data = model.data(model.index(r), Qt::DisplayRole);
        visibleNames.insert(data.toString());
    }

    ASSERT_EQ(separatorNumber, 2);

    for (auto& eventParameter: kEvenParameterList)
    {
        ASSERT_TRUE(visibleNames.contains(
            nx::vms::rules::utils::EventParameterHelper::addBrackets(eventParameter)));
    }
}

void whenExpandGroup(EventParametersModel& model, const QString& group)
{
    model.expandGroup(group);
}

void whenResetingExpandedGroup(EventParametersModel& model)
{
    model.resetExpandedGroup();
}

EventParametersModel givenEventParameterModel(
    const std::optional<EventParameterHelper::EventParametersNames>& eventParameterList = {})
{
    return {eventParameterList ? eventParameterList.value() : kEvenParameterList};
}


TEST(EventParametersTests, EventParametersModelBuild)
{
    auto model = givenEventParameterModel();
    // Check that subgroup are hidden + 1 separator line is presented.
    thenModelIsInDefaultState(model);
}

TEST(EventParametersTests, EventParametersModelExpandSubGroupName)
{
    auto model = givenEventParameterModel();

    whenExpandGroup(model, "event.group1");
    // Check that subgroup are expanded + 1 separator line is presented.
    thenIsExpanded(model);
    // Check that after reset, the number of rows as in default example.
    whenResetingExpandedGroup(model);
    thenModelIsInDefaultState(model);
}

TEST(EventParametersTests, EventParametersModelExpandBracketOpenName)
{
    auto model = givenEventParameterModel();
    whenExpandGroup(model,"{event.group1");
    // Check that subgroup are expanded + 1 separator line is presented.
    thenIsExpanded(model);
}

TEST(EventParametersTests, EventParametersModelIsSubGroupName)
{
    auto model = givenEventParameterModel();
    ASSERT_TRUE(model.isSubGroupName("{event.group1}"));
    ASSERT_FALSE(model.isSubGroupName("{event.group1.sub_arg"));
}

TEST(EventParametersTests, EventParametersModelExpandSubGroupValue)
{
    auto model = givenEventParameterModel();
    whenExpandGroup(model, "event.group1.na");
    thenNumberOfRowsEqualToExpanded(model);
    thenIsExpanded(model);
}

TEST(EventParametersTests, EventParametersModelExpandNonSubGroup)
{
    auto model = givenEventParameterModel();
    whenExpandGroup(model, "event.value");
    thenModelIsInDefaultState(model);
}

TEST(EventParametersTests, EventParametersEmptyArgs)
{
    auto model = givenEventParameterModel();
    whenExpandGroup(model, "event.value");
    thenModelIsInDefaultState(model);
    ASSERT_FALSE(model.isSubGroupName(""));
}

TEST(EventParametersTests, EventParametersEmptyModel)
{
    auto model = givenEventParameterModel(EventParameterHelper::EventParametersNames());
    model.expandGroup("");
    thenRowCountIs(model, 0);
    ASSERT_FALSE(model.isSubGroupName(""));
}

TEST(EventParametersTests, EventParametersModelResetWithoutExpand)
{
    auto model = givenEventParameterModel();
    const auto rowCountBefore = model.rowCount({});
    whenResetingExpandedGroup(model);
    thenRowCountIs(model, rowCountBefore);
    thenModelIsInDefaultState(model);
}

TEST(EventParametersTests, EventParametersModelFilter)
{
    auto model = givenEventParameterModel();

    auto whenSetFilter =
        [&](const QString& filter)
        {
            model.setDefaultVisibleElements();
            model.filter(filter);
        };

    // Set filter to begining of the event parameter.
    whenSetFilter("{");

    // Check that model contains default values.
    thenModelIsInDefaultState(model);

    // Setting filter to one correct value.
    whenSetFilter("{event.value");

    // Check that model contains only that value.
    thenRowCountIs(model, 1);

    // Set filter to begining of group.
    whenSetFilter("{group");
    thenNumberOfGroupsIs(model, 1);

    // Set filter to incorrect value, starting with bracket.
    whenSetFilter("{trulala");
    thenRowCountIs(model, 0);

    // Set filter to incorrect value, without bracket.
    whenSetFilter("bubaba");
    thenRowCountIs(model, 0);
}
} // namespace nx::vms::rules::test
