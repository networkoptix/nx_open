// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "taxonomy_based_ut.h"

#include <QtCore/QFile>

#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/json/serializer.h>
#include <nx/vms/client/desktop/analytics/taxonomy/test_support/test_resource_support_proxy.h>

using namespace nx::vms::client::desktop::analytics::taxonomy;
namespace nx::vms::client::desktop {


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

nx::vms::api::analytics::Descriptors prepareDescriptors(
    const InputData& inputData, const nx::vms::api::analytics::Descriptors& commonDescriptors)
{
    api::analytics::Descriptors descriptors = commonDescriptors;
    descriptors.objectTypeDescriptors.clear();
    for (const QString& objectTypeId: inputData.objectTypes)
    {
        const auto itr = commonDescriptors.objectTypeDescriptors.find(objectTypeId);
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

void TaxonomyBasedTest::SetUpTestSuite()
{
    QFile dataFile(":/taxonomy_based_ut/test_data.json");
    ASSERT_TRUE(dataFile.open(QIODevice::ReadOnly));

    s_inputData = std::make_unique<TestData>();
    ASSERT_TRUE(nx::reflect::json::deserialize(nx::Buffer(dataFile.readAll()), s_inputData.get()));
}

void TaxonomyBasedTest::TearDownTestSuite()
{
    s_inputData.reset();
}

TaxonomyBasedTest::TaxonomyBasedTest()
{
    const auto m_descriptors = prepareDescriptors(s_inputData->input, s_inputData->descriptors);
    std::unique_ptr<AbstractResourceSupportProxy> resourceSupportProxy =
        prepareResourceSupportProxy(s_inputData->input.attributeSupportInfo);
    const nx::analytics::taxonomy::StateCompiler::Result result =
        nx::analytics::taxonomy::StateCompiler::compile(
            m_descriptors, std::move(resourceSupportProxy));

    m_stateViewBuilder = std::make_unique<StateViewBuilder>(result.state);
}

StateView* TaxonomyBasedTest::stateView()
{
    return m_stateViewBuilder->stateView();
}

} // namespace nx::vms::client::desktop
