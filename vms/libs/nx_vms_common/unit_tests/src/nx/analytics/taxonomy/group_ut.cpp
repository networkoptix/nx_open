// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QFile>

#include <nx/analytics/taxonomy/support/utils.h>
#include <nx/analytics/taxonomy/state_compiler.h>

#include <nx/fusion/model_functions.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

class GroupTest: public ::testing::Test
{
protected:
    void givenDescriptors(const QString& filePath)
    {
        TestData testData;
        ASSERT_TRUE(loadDescriptorsTestData(filePath, &testData));
        m_descriptors = std::move(testData.descriptors);
        ASSERT_TRUE(QJson::deserialize(testData.fullData["result"].toObject(), &m_expectedData));
        ASSERT_FALSE(m_expectedData.empty());
    }

    void afterDescriptorsCompilation()
    {
        m_result = StateCompiler::compile(m_descriptors);
    }

    void makeSureGroupsAreCorrect()
    {
        const std::vector<AbstractGroup*> groups = m_result.state->groups();

        ASSERT_EQ(groups.size(), m_expectedData.size());

        for (const AbstractGroup* group: groups)
        {
            GroupDescriptor expectedData = m_expectedData[group->id()];

            ASSERT_EQ(group->id(), expectedData.id);
            ASSERT_EQ(group->name(), expectedData.name);
        }
    }

private:
    Descriptors m_descriptors;
    std::map<QString, GroupDescriptor> m_expectedData;
    StateCompiler::Result m_result;
};

TEST_F(GroupTest, groupsAreCorrect)
{
    givenDescriptors(":/content/taxonomy/group_test.json");
    afterDescriptorsCompilation();
    makeSureGroupsAreCorrect();
}

} // namespace nx::analytics::taxoonomy
