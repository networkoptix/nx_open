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
    "event.group1", "event.group1.name", "event.group1.name2", "event.value", "group2.parameter"};

void checkIsDefaultView(const EventParametersModel& model)
{
    ASSERT_EQ(model.rowCount({}), 4);

    int separatorNumber = 0;
    for (int r = 0; r < model.rowCount({}); ++r)
    {
        auto el = model.data(model.index(r), Qt::DisplayRole);
        ASSERT_NE(el.toString(), "event.group1.name");
        ASSERT_NE(el.toString(), "event.group1.name2");
        auto accessRole = model.data(model.index(r), Qt::AccessibleDescriptionRole);
        if (accessRole.canConvert<Separator>())
            separatorNumber += 1;
    }

    ASSERT_EQ(separatorNumber, 1);
}

void checkIsExpanded(const EventParametersModel& model)
{
    ASSERT_EQ(model.rowCount({}), 6);

    EventParameterHelper::EventParametersNames visibleNames;
    int separatorNumber = 0;
    for (int r = 0; r < model.rowCount({}); ++r)
    {
        auto accessRole = model.data(model.index(r), Qt::AccessibleDescriptionRole);
        if (accessRole.canConvert<Separator>())
        {
            separatorNumber += 1;
            continue;
        }
        auto el = model.data(model.index(r), Qt::DisplayRole);
        visibleNames.push_back(el.toString());
    }

    ASSERT_EQ(separatorNumber, 1);

    for (auto& el: kEvenParameterList)
    {
        ASSERT_TRUE(
            visibleNames.contains(nx::vms::rules::utils::EventParameterHelper::addBrackets(el)));
    }
}

TEST(EventParametersTests, EventParametersModelBuild)
{
    EventParametersModel model(kEvenParameterList);
    // Check that subgroup are hidden + 1 separator line is presented.
    checkIsDefaultView(model);
}

TEST(EventParametersTests, EventParametersModelExpandSubGroupName)
{
    EventParametersModel model(kEvenParameterList);
    model.expandGroup("event.group1");
    // Check that subgroup are expanded + 1 separator line is presented.
    checkIsExpanded(model);
    // Check that after reset, the number of rows as in default example.
    model.resetExpandedGroup();
    checkIsDefaultView(model);
}

TEST(EventParametersTests, EventParametersModelExpandBracketOpenName)
{
    EventParametersModel model(kEvenParameterList);
    model.expandGroup("{event.group1");
    // Check that subgroup are expanded + 1 separator line is presented.
    checkIsExpanded(model);
}

TEST(EventParametersTests, EventParametersModelIsSubGroupName)
{
    EventParametersModel model(kEvenParameterList);
    ASSERT_TRUE(model.isSubGroupName("{event.group1}"));
    ASSERT_FALSE(model.isSubGroupName("{event.group1.name"));
}

TEST(EventParametersTests, EventParametersModelExpandSubGroupValue)
{
    EventParametersModel model(kEvenParameterList);
    model.expandGroup("event.group1.na");
    ASSERT_EQ(model.rowCount({}), 6);
    checkIsExpanded(model);
}

TEST(EventParametersTests, EventParametersModelExpandNonSubGroup)
{
    EventParametersModel model(kEvenParameterList);
    model.expandGroup("event.value");
    ASSERT_EQ(model.rowCount({}), 4);
    checkIsDefaultView(model);
}

TEST(EventParametersTests, EventParametersEmptyArgs)
{
    EventParametersModel model(kEvenParameterList);
    model.expandGroup("");
    ASSERT_EQ(model.rowCount({}), 4);
    checkIsDefaultView(model);
    ASSERT_FALSE(model.isSubGroupName(""));
}

TEST(EventParametersTests, EventParametersEmptyModel)
{
    EventParametersModel model({});
    model.expandGroup("");
    ASSERT_EQ(model.rowCount({}), 0);
    ASSERT_FALSE(model.isSubGroupName(""));
}

TEST(EventParametersTests, EventParametersModelResetWithoutExpand)
{
    EventParametersModel model(kEvenParameterList);
    const auto rowCountBefore = model.rowCount({});
    model.resetExpandedGroup();
    ASSERT_EQ(rowCountBefore, model.rowCount({}));
    checkIsDefaultView(model);
}
} // namespace nx::vms::rules::test
