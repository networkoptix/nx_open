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

const EventParameterHelper::EventParametersNames kNotExpandedParameterList = {
    "group0.parameter",
    "event.group1",  "event.value",
    "group2.parameter",
    "group3.subgroup"};

const EventParameterHelper::EventParametersNames kFullEventParameterList = {
    "group0.parameter",
    // In not expanded state contains values [event.group1, "event.value"]
    "event.group1", "event.group1.sub_arg", "event.group1.sub_arg2", "event.value",
    "group2.parameter",
    // In not expanded state contains values [group3.subgroup]
    "group3.subgroup.sub_arg", "group3.subgroup.space value"};

const QSet<QString> kHiddenValuesByDefault = {
    "event.group1.sub_arg",
    "event.group1.sub_arg2",
    "group3.subgroup.sub_arg",
    "group3.subgroup.space value"};

const std::map<QString, std::vector<QString>> kValuesByGroup = {
    {"group0", {"group0.parameter"}},
    {"group2", {"group2.parameter"}},
    {"event", {"event.group1", "event.group1.sub_arg", "event.group1.sub_arg2", "event.value"}},
    {"group3", {"group3.subgroup.sub_arg", "group3.subgroup.space value"}}
};

const int kNumberOfGroups = kValuesByGroup.size() - 1; // Last separator is deleted
const int kNotExpandedElementsNumber = kNumberOfGroups + kNotExpandedParameterList.size();

class EventParametersTest: public ::testing::Test
{
public:
    EventParametersTest() = default;

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
        ASSERT_EQ(model.rowCount({}), kNotExpandedElementsNumber);

        int separatorNumber = 0;
        for (int r = 0; r < model.rowCount({}); ++r)
        {
            const auto el = model.data(model.index(r), Qt::DisplayRole);
            ASSERT_FALSE(kHiddenValuesByDefault.contains(el.toString()));
            auto accessRole = model.data(model.index(r), Qt::AccessibleDescriptionRole);
            if (accessRole.canConvert<Separator>())
                separatorNumber += 1;
        }

        ASSERT_EQ(separatorNumber, kNumberOfGroups);
    }

    void thenIsExpanded(const EventParametersModel& model, const QString& groupName)
    {
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

        ASSERT_EQ(separatorNumber, kNumberOfGroups);

        ASSERT_TRUE(kValuesByGroup.contains(groupName));

        for (auto& eventParameter: kValuesByGroup.at(groupName))
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
        const EventParameterHelper::EventParametersNames& eventParameterList = kFullEventParameterList)
    {
        return {eventParameterList};
    }
};

TEST_F(EventParametersTest, EventParametersModelBuild)
{
    auto model = givenEventParameterModel();
    // Check that subgroups are hidden.
    thenModelIsInDefaultState(model);
}

TEST_F(EventParametersTest, EventParametersModelExpandSubGroupName)
{
    auto model = givenEventParameterModel();

    whenExpandGroup(model, "event.group1");
    // Check that subgroup is expanded.
    thenIsExpanded(model, "event");
    whenResetingExpandedGroup(model);
    thenModelIsInDefaultState(model);
}

TEST_F(EventParametersTest, EventParametersModelExpandBracketOpenName)
{
    auto model = givenEventParameterModel();
    whenExpandGroup(model,"{event.group1");
    thenIsExpanded(model, "event");
}

TEST_F(EventParametersTest, EventParametersModelIsSubGroupName)
{
    auto model = givenEventParameterModel();
    ASSERT_TRUE(model.isSubGroupName("{event.group1}"));
    ASSERT_FALSE(model.isSubGroupName("{event.group1.sub_arg"));
}

TEST_F(EventParametersTest, EventParametersModelExpandSubGroupValue)
{
    auto model = givenEventParameterModel();
    whenExpandGroup(model, "event.group1.na");
    thenIsExpanded(model, "event");
}

TEST_F(EventParametersTest, EventParametersModelExpandNonSubGroup)
{
    auto model = givenEventParameterModel();
    whenExpandGroup(model, "event.value");
    thenModelIsInDefaultState(model);
}

TEST_F(EventParametersTest, EventParametersEmptyArgs)
{
    auto model = givenEventParameterModel();
    whenExpandGroup(model, "event.value");
    thenModelIsInDefaultState(model);
    ASSERT_FALSE(model.isSubGroupName(""));
}

TEST_F(EventParametersTest, EventParametersEmptyModel)
{
    auto model = givenEventParameterModel(EventParameterHelper::EventParametersNames());
    model.expandGroup("");
    thenRowCountIs(model, 0);
    ASSERT_FALSE(model.isSubGroupName(""));
}

TEST_F(EventParametersTest, EventParametersModelResetWithoutExpand)
{
    auto model = givenEventParameterModel();
    const auto rowCountBefore = model.rowCount({});
    whenResetingExpandedGroup(model);
    thenRowCountIs(model, rowCountBefore);
    thenModelIsInDefaultState(model);
}

TEST_F(EventParametersTest, EventParametersModelCaseInsesitive)
{
    auto model = givenEventParameterModel();
    ASSERT_TRUE(model.isCorrectParameter("event.group1"));
    ASSERT_TRUE(model.isCorrectParameter("EVENT.group1"));
    ASSERT_TRUE(model.isCorrectParameter("event.GROUP1"));
    ASSERT_TRUE(model.isCorrectParameter("group3.subgroup.space value"));
    ASSERT_TRUE(model.isCorrectParameter("group3.subgroup.space Value"));
    ASSERT_TRUE(model.isCorrectParameter("group3.subgroup.Space value"));
    ASSERT_TRUE(model.isCorrectParameter("group3.SUBGROUP.space value"));
}

TEST_F(EventParametersTest, EventParametersModelFilter)
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
    thenNumberOfGroupsIs(model, 2);

    // Set filter to begining of group in differnt case.
    whenSetFilter("{GROUP");
    thenNumberOfGroupsIs(model, 2);

    // Set filter to incorrect value, starting with bracket.
    whenSetFilter("{trulala");
    thenRowCountIs(model, 0);

    // Set filter to incorrect value, without bracket.
    whenSetFilter("bubaba");
    thenRowCountIs(model, 0);
}
} // namespace nx::vms::rules::test
