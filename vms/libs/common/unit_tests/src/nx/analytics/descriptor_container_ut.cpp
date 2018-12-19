#include <gtest/gtest.h>

#include <string>

#include <nx/analytics/descriptor_container.h>

#include <nx/analytics/test_descriptor.h>
#include <nx/analytics/test_descriptor_storage.h>
#include <nx/analytics/test_merge_executor.h>

namespace nx::analytics::test{

using namespace nx::utils::data_structures;

using Storage = TestDescriptorStorage<TestDescriptor, std::string, std::string, std::string>;
using MergeExecutor = TestMergeExecutor;

void initMap(Storage::Container* map)
{
    MapHelper::set(map, "group0", "subgroup0", "id0", TestDescriptor("id0", "descriptor0-0-0"));
    MapHelper::set(map, "group0", "subgroup1", "id1", TestDescriptor("id1", "descriptor0-1-1"));
    MapHelper::set(map, "group1", "subgroup0", "id0", TestDescriptor("id0", "descriptor1-0-0"));
    MapHelper::set(map, "group1", "subgroup1", "id1", TestDescriptor("id1", "descriptor1-1-1"));
}

class DescriptorContainerTest: public ::testing::Test
{

protected:
    virtual void SetUp() override
    {
        Storage::Container descriptors;
        initMap(&descriptors);

        auto storage = std::make_unique<Storage>();
        storage->save(std::move(descriptors));

        m_descriptorContainer =
            std::make_unique<DescriptorContainer<Storage, MergeExecutor>>(std::move(storage));
    }

protected:
    std::unique_ptr<DescriptorContainer<Storage, MergeExecutor>> m_descriptorContainer;
};

TEST_F(DescriptorContainerTest, getDescriptors)
{
    Storage::Container expectedResult0;
    initMap(&expectedResult0);
    const auto result0 = m_descriptorContainer->descriptors();
    ASSERT_EQ(expectedResult0, *result0);

    MapHelper::NestedMap<std::map, std::string, std::string, TestDescriptor> expectedResult1;
    MapHelper::set(&expectedResult1, "subgroup0", "id0", TestDescriptor("id0", "descriptor0-0-0"));
    MapHelper::set(&expectedResult1, "subgroup1", "id1", TestDescriptor("id1", "descriptor0-1-1"));

    const auto result1 = m_descriptorContainer->descriptors("group0");
    ASSERT_EQ(expectedResult1, *result1);

    MapHelper::NestedMap<std::map, std::string, TestDescriptor> expectedResult2;
    MapHelper::set(&expectedResult2, "id0", TestDescriptor("id0", "descriptor0-0-0"));

    const auto result2 = m_descriptorContainer->descriptors("group0", "subgroup0");
    ASSERT_EQ(expectedResult2, *result2);

    const TestDescriptor expectedResult3("id0", "descriptor0-0-0");
    const auto result3 = m_descriptorContainer->descriptors("group0", "subgroup0", "id0");
    ASSERT_EQ(expectedResult3, *result3);
}

TEST_F(DescriptorContainerTest, setDescriptors)
{
    Storage::Container expectedResult0;
    initMap(&expectedResult0);
    MapHelper::set(
        &expectedResult0, "group0", "subgroup2", "id1", TestDescriptor("id1", "descriptor0-2-1"));

    m_descriptorContainer->setDescriptors(
        TestDescriptor("id1", "descriptor0-2-1"), "group0", "subgroup2", "id1");

    ASSERT_EQ(expectedResult0, *m_descriptorContainer->descriptors());

    m_descriptorContainer->setDescriptors(Storage::Container());
    ASSERT_EQ(Storage::Container(), *m_descriptorContainer->descriptors());

    Storage::Container expectedResult1;
    initMap(&expectedResult1);

    MapHelper::Unwrapped<Storage::Container> subMap1;
    MapHelper::set(&subMap1, "subgroup0", "id0", TestDescriptor("id0", "descriptor0-0-0"));
    MapHelper::set(&subMap1, "subgroup1", "id1", TestDescriptor("id1", "descriptor0-1-1"));

    m_descriptorContainer->setDescriptors(
        TestDescriptor("id0", "descriptor1-0-0"), "group1", "subgroup0", "id0");

    m_descriptorContainer->setDescriptors(
        TestDescriptor("id1", "descriptor1-1-1"), "group1", "subgroup1", "id1");

    m_descriptorContainer->setDescriptors(subMap1, "group0");
    ASSERT_EQ(expectedResult1, *m_descriptorContainer->descriptors());
}

TEST_F(DescriptorContainerTest, addDescriptors)
{
    Storage::Container expectedResult0;
    initMap(&expectedResult0);
    auto& descriptor = MapHelper::get(expectedResult0, "group0", "subgroup1", "id1");
    descriptor.mergeCounter += 1;

    m_descriptorContainer->addDescriptors(
        TestDescriptor("id1", "descriptor0-1-1"), "group0", "subgroup1", "id1");

    ASSERT_EQ(expectedResult0, *m_descriptorContainer->descriptors());
}

TEST_F(DescriptorContainerTest, removeDescriptors)
{
    Storage::Container expectedResult0;
    MapHelper::set(
        &expectedResult0, "group0", "subgroup0", "id0", TestDescriptor("id0", "descriptor0-0-0"));
    MapHelper::set(
        &expectedResult0, "group0", "subgroup1", "id1", TestDescriptor("id1", "descriptor0-1-1"));
    MapHelper::set(
        &expectedResult0, "group1", "subgroup0", "id0", TestDescriptor("id0", "descriptor1-0-0"));

    m_descriptorContainer->removeDescriptors("group1", "subgroup1", "id1");
    ASSERT_EQ(expectedResult0, *m_descriptorContainer->descriptors());

    Storage::Container expectedResult1;
    MapHelper::set(
        &expectedResult1, "group1", "subgroup0", "id0", TestDescriptor("id0", "descriptor1-0-0"));
    m_descriptorContainer->removeDescriptors("group0");
    ASSERT_EQ(expectedResult1, *m_descriptorContainer->descriptors());

    m_descriptorContainer->removeDescriptors("group1", "subgroup0");
    ASSERT_EQ(Storage::Container(), *m_descriptorContainer->descriptors());
}

} // namespace nx::analytics::test
