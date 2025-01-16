// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtTest/QAbstractItemModelTester>

#include <nx/analytics/taxonomy/object_type.h>
#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/vms/client/core/analytics/taxonomy/object_type.h>
#include <nx/vms/client/desktop/analytics/taxonomy/taxonomy_test_data.h>
#include <nx/vms/client/desktop/analytics/taxonomy/test_support/test_resource_support_proxy.h>
#include <nx/vms/client/desktop/event_rules/models/detectable_object_type_model.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

class TestFilterModel: public core::analytics::taxonomy::AnalyticsFilterModel
{
    using base_type = core::analytics::taxonomy::AnalyticsFilterModel;
    friend class DetectableObjectTypeModelTest;
};

class DetectableObjectTypeModelTest: public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        m_filterModel.reset(new TestFilterModel());
        m_testModel.reset(new DetectableObjectTypeModel(m_filterModel.get()));
        m_modelTester.reset(new QAbstractItemModelTester(m_testModel.get(),
            QAbstractItemModelTester::FailureReportingMode::Fatal));
    }

    virtual void TearDown() override
    {
        m_modelTester.reset();
        m_testModel.reset();
        m_filterModel.reset();
    }

protected:
    static void SetUpTestSuite()
    {
        QFile dataFile(":/taxonomy_based_ut/detectable_object_type_model_test_data.json");
        ASSERT_TRUE(dataFile.open(QIODevice::ReadOnly));

        s_testData = std::make_unique<TestData>();
        ASSERT_TRUE(
            nx::reflect::json::deserialize(nx::Buffer(dataFile.readAll()), s_testData.get()));
    }

    void givenDescriptors(const QString& testCaseName)
    {
        using namespace nx::analytics::taxonomy;

        const auto itr = s_testData->testCases.find(testCaseName);
        ASSERT_FALSE(itr == s_testData->testCases.cend());

        m_testCase = itr->second;
        m_descriptors = prepareDescriptors(m_testCase.input, s_testData->descriptors);
        m_resourceSupportProxy =
            std::make_unique<TestResourceSupportProxy>(m_testCase.input.attributeSupportInfo);
        m_attributeValues = m_testCase.input.attributeValues;
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

    void afterStateViewIsBuilt()
    {
        const nx::analytics::taxonomy::StateCompiler::Result result =
            nx::analytics::taxonomy::StateCompiler::compile(
                m_descriptors,
                std::move(m_resourceSupportProxy));

        m_stateViewBuilder = std::make_unique<StateViewBuilder>(result.state);
        m_filterModel->setObjectTypes(m_stateViewBuilder->stateView()->objectTypes());
    }

private:
    static std::map<QString, std::set<nx::Uuid>> toDescriptorAttributeSupportInfo(
        const InputData::AttributeSupportInfo& inputDataAttributeSupportInfo)
    {
        std::map<QString, std::set<nx::Uuid>> result;
        for (const auto& [attributeName, supportInfoByEngineAndDevice]: inputDataAttributeSupportInfo)
        {
            for (const auto& [engineId, deviceIds]: supportInfoByEngineAndDevice)
                result[attributeName].insert(engineId);
        }

        return result;
    }

protected:
    std::unique_ptr<DetectableObjectTypeModel> m_testModel;
    std::unique_ptr<TestFilterModel> m_filterModel;
    std::unique_ptr<StateViewBuilder> m_stateViewBuilder;
    TestCase m_testCase;

private:
    std::unique_ptr<QAbstractItemModelTester> m_modelTester;
    static inline std::unique_ptr<TestData> s_testData = nullptr;
    nx::vms::api::analytics::Descriptors m_descriptors;
    std::optional<std::map<QString, QString>> m_attributeValues;
    std::unique_ptr<nx::analytics::taxonomy::AbstractResourceSupportProxy> m_resourceSupportProxy;
};

TEST_F(DetectableObjectTypeModelTest, basics)
{
    givenDescriptors("basics");
    afterStateViewIsBuilt();
    auto objectTypes = m_stateViewBuilder->stateView()->objectTypes();
    ASSERT_EQ(m_testModel->rowCount({}), objectTypes.size());
    ASSERT_EQ(m_filterModel->objectTypes().size(), objectTypes.size());

    for (int i = 0; i < m_testModel->rowCount({}); ++i)
    {
        const auto index = m_testModel->index(i, 0, {});
        ASSERT_EQ(
            index.data(DetectableObjectTypeModel::NameRole).toString(), objectTypes[i]->name());
        ASSERT_EQ(
            index.data(DetectableObjectTypeModel::MainIdRole).toString(),
            objectTypes[i]->mainTypeId());
        ASSERT_EQ(
            index.data(DetectableObjectTypeModel::IdsRole).toStringList(),
            m_filterModel->getAnalyticsObjectTypeIds(objectTypes[i]));
    }
}

TEST_F(DetectableObjectTypeModelTest, changeTaxonomy)
{
    givenDescriptors("basics");
    afterStateViewIsBuilt();
    auto objectTypes = m_stateViewBuilder->stateView()->objectTypes();

    ASSERT_EQ(m_testModel->rowCount({}), objectTypes.size());

    givenDescriptors("alternative");
    afterStateViewIsBuilt();
    ASSERT_NE(m_testModel->rowCount({}), objectTypes.size());
}

TEST_F(DetectableObjectTypeModelTest, derivedTypes)
{
    givenDescriptors("derivedTypes");
    afterStateViewIsBuilt();

    std::function<void(std::vector<ExpectedObjectType>, QModelIndex)> checkRecursive =
        [&](std::vector<ExpectedObjectType> expectedObjectTypes, QModelIndex parentIndex)
        {
            ASSERT_EQ(m_testModel->rowCount(parentIndex), expectedObjectTypes.size());
            for (int i = 0; i < expectedObjectTypes.size(); ++i)
            {
                const auto index = m_testModel->index(i, 0, parentIndex);
                const auto expectedObjectType = expectedObjectTypes[i];
                ASSERT_EQ(
                    index.data(DetectableObjectTypeModel::NameRole).toString(),
                    expectedObjectType.name);
                checkRecursive(expectedObjectType.objectTypes, index);
            }
        };

    checkRecursive(m_testCase.expected.objectTypes, {});
}

} // namespace nx::vms::client::desktop
