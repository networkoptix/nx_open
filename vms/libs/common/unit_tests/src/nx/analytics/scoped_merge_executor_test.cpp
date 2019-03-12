#include <gtest/gtest.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/analytics/scoped_merge_executor.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

template<typename Descriptor>
struct TestCase
{
    std::optional<Descriptor> first;
    std::optional<Descriptor> second;
    std::optional<Descriptor> result;
};

template<typename Descriptor>
Descriptor makeDescriptor(
    std::set<DescriptorScope> scopes,
    QString id = "defaultId",
    QString name = "defaultName")
{
    Descriptor descriptor;
    descriptor.id = std::move(id);
    descriptor.name = std::move(name);
    descriptor.scopes = std::move(scopes);
    return descriptor;
}

template<typename Descriptor>
std::vector<TestCase<Descriptor>> makeTestCases()
{
    return {
        {
            makeDescriptor<Descriptor>(std::set<DescriptorScope>{}),
            makeDescriptor<Descriptor>(std::set<DescriptorScope>{}),
            makeDescriptor<Descriptor>(std::set<DescriptorScope>{}),
        },
        {
            makeDescriptor<Descriptor>(
                {{QnUuid::fromArbitraryData(QString("someEngine")), "someGroup"}}),
            makeDescriptor<Descriptor>(
                {{QnUuid::fromArbitraryData(QString("someEngine")), "someGroup"}}),
            makeDescriptor<Descriptor>(
                {{QnUuid::fromArbitraryData(QString("someEngine")), "someGroup"}}),
        },
        {
            makeDescriptor<Descriptor>(
                {{QnUuid::fromArbitraryData(QString("someEngine")), "someGroup"}}),
            makeDescriptor<Descriptor>(
                {{QnUuid::fromArbitraryData(QString("someOtherEngine")), "someGroup"}}),
            makeDescriptor<Descriptor>(
                {
                    {QnUuid::fromArbitraryData(QString("someEngine")), "someGroup"},
                    {QnUuid::fromArbitraryData(QString("someOtherEngine")), "someGroup"}
                })
        },
        {
            std::nullopt,
            std::nullopt,
            std::nullopt
        },
        {
            std::nullopt,
            makeDescriptor<Descriptor>(
                {{QnUuid::fromArbitraryData(QString("someEngine")), "someGroup"}}),
            makeDescriptor<Descriptor>(
                {{QnUuid::fromArbitraryData(QString("someEngine")), "someGroup"}}),
        },
        {
            makeDescriptor<Descriptor>(
                {{QnUuid::fromArbitraryData(QString("someEngine")), "someGroup"}}),
            std::nullopt,
            makeDescriptor<Descriptor>(
                {{QnUuid::fromArbitraryData(QString("someEngine")), "someGroup"}}),
        },
        {
            makeDescriptor<Descriptor>(
                {
                    {QnUuid::fromArbitraryData(QString("someEngine")), "someGroup"},
                    {QnUuid::fromArbitraryData(QString("someOtherEngine")), "someGroup"}
                }),
            makeDescriptor<Descriptor>(
                {
                    {QnUuid::fromArbitraryData(QString("someEngine2")), "someGroup"},
                    {QnUuid::fromArbitraryData(QString("someOtherEngine2")), "someGroup"}
                }),
            makeDescriptor<Descriptor>(
                {
                    {QnUuid::fromArbitraryData(QString("someEngine")), "someGroup"},
                    {QnUuid::fromArbitraryData(QString("someOtherEngine")), "someGroup"},
                    {QnUuid::fromArbitraryData(QString("someEngine2")), "someGroup"},
                    {QnUuid::fromArbitraryData(QString("someOtherEngine2")), "someGroup"}
                })
        },
        {
            makeDescriptor<Descriptor>(
                {{QnUuid::fromArbitraryData(QString("someEngine")), "someGroup"}}),
            makeDescriptor<Descriptor>(
                {{QnUuid::fromArbitraryData(QString("someEngine")), "someGroup2"}}),
            makeDescriptor<Descriptor>(
                {{QnUuid::fromArbitraryData(QString("someEngine")), "someGroup2"}}),
        }
    };
}

template<typename Descriptor>
void runTest()
{
    const ScopedMergeExecutor<Descriptor> merger;
    for (const auto testCase : makeTestCases<Descriptor>())
    {
        auto merged = merger(
            testCase.first ? &(*testCase.first) : nullptr,
            testCase.second ? &(*testCase.second) : nullptr);

        ASSERT_EQ(merged, testCase.result);
    }
}

TEST(ScopedMergeExecutorTest, eventTypeDescriptor)
{
    runTest<EventTypeDescriptor>();
}

TEST(ScopedMergeExecutorTest, objectTypeDescriptor)
{
    runTest<ObjectTypeDescriptor>();
}

} // namespace nx::analytics
