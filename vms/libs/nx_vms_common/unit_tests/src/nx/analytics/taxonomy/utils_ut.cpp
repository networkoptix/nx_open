// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QFile>

#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/analytics/taxonomy/utils.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/vms/common/test_support/analytics/taxonomy/test_resource_support_proxy.h>
#include <nx/vms/common/test_support/analytics/taxonomy/utils.h>

namespace nx::analytics::taxonomy {

using namespace nx::vms::api::analytics;

struct TestCaseData
{
    QString objectType;
    QString attribute;
    bool shouldBeFound = true;
    QString expectedName;
};
NX_REFLECTION_INSTRUMENT(TestCaseData, (objectType)(attribute)(shouldBeFound)(expectedName))

class FindAttributeTest: public ::testing::TestWithParam<QString>
{
public:
    virtual void SetUp() override
    {
        TestData testData;
        ASSERT_TRUE(
            loadDescriptorsTestData(":/content/taxonomy/find_attribute_test.json", &testData));
        const Descriptors descriptors = std::move(testData.descriptors);

        StateCompiler::Result result = StateCompiler::compile(
            descriptors, std::make_unique<TestResourceSupportProxy>());
        ASSERT_TRUE(result.errors.empty());
        m_state = result.state;

        const QJsonObject testsJson = testData.fullData["tests"].toObject();

        nx::reflect::json::deserialize(
            QJsonDocument(testsJson).toJson().toStdString(), &m_testCases);
        ASSERT_FALSE(m_testCases.empty());
    }

    void checkTestCase(const QString& name)
    {
        const auto testIt = m_testCases.find(name);
        ASSERT_FALSE(testIt == m_testCases.end());

        const TestCaseData& test = testIt->second;

        const AbstractAttribute* attribute = findAttributeByName(
            m_state.get(), test.objectType, test.attribute);

        if (test.shouldBeFound)
        {
            ASSERT_NE(attribute, nullptr);
            QString expectedName = test.expectedName;
            if (expectedName.isEmpty())
                expectedName = test.attribute;
            ASSERT_EQ(expectedName, attribute->name());
        }
        else
        {
            ASSERT_EQ(attribute, nullptr);
        }
    }

public:
    std::shared_ptr<AbstractState> m_state;
    std::map<QString, TestCaseData> m_testCases;
};

TEST_P(FindAttributeTest, check)
{
    checkTestCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    TaxonomyUtils,
    FindAttributeTest,
    ::testing::Values(
        "simpleLookup",
        "missingAttribute",
        "lookupInTheBaseObject",
        "lookupNestedAttribute"
    ),
    ::testing::PrintToStringParamName());

} // namespace nx::analytics::taxonomy
