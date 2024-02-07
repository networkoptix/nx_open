// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/analytics/taxonomy/state_view.h>
#include <nx/vms/client/desktop/analytics/taxonomy/state_view_builder.h>

using nx::analytics::taxonomy::AbstractResourceSupportProxy;
namespace nx::vms::client::desktop {

struct InputData
{
    using AttributeSupportInfo = std::map<QString /*attributeName*/,
        std::map<QnUuid /*engineId*/, std::set<QnUuid /*deviceId*/>>>;

    std::vector<QString> objectTypes;
    std::map<QString, AttributeSupportInfo> attributeSupportInfo;
};
#define InputData_Fields (objectTypes)(attributeSupportInfo)
NX_REFLECTION_INSTRUMENT(InputData, InputData_Fields);

struct TestData
{
    api::analytics::Descriptors descriptors;
    InputData input;
};
#define TestData_Fields (descriptors)(input)
NX_REFLECTION_INSTRUMENT(TestData, TestData_Fields);

class TaxonomyBasedTest: public ::testing::Test
{
public:
    TaxonomyBasedTest();
    static void SetUpTestSuite();
    static void TearDownTestSuite();
    analytics::taxonomy::StateView* stateView();

private:
    std::unique_ptr<analytics::taxonomy::StateViewBuilder> m_stateViewBuilder;
    static inline std::unique_ptr<TestData> s_inputData = nullptr;
};
} // namespace nx::vms::client::desktop
