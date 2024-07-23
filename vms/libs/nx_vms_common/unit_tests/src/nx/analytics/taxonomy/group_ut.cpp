// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QFile>

#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/vms/common/test_support/analytics/taxonomy/test_resource_support_proxy.h>
#include <nx/vms/common/test_support/analytics/taxonomy/utils.h>

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

        const QJsonObject object = testData.fullData["result"].toObject();
        const QByteArray objectAsBytes = QJsonDocument(object).toJson();

        auto [deserializationResult, result] =
            nx::reflect::json::deserialize<std::map<QString, GroupDescriptor>>(objectAsBytes.toStdString());

        m_expectedData = deserializationResult;

        ASSERT_TRUE(result.success);

        ASSERT_FALSE(m_expectedData.empty());
    }

    void afterDescriptorsCompilation()
    {
        m_result = StateCompiler::compile(
            m_descriptors,
            std::make_unique<TestResourceSupportProxy>());
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
