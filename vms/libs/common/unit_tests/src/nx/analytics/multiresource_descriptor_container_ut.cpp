#include <gtest/gtest.h>

#include <nx/analytics/test_merge_executor.h>
#include <nx/analytics/test_descriptor_storage_factory.h>

#include <nx/analytics/property_descriptor_storage_factory.h>
#include <nx/analytics/multiresource_descriptor_container.h>
#include <nx/utils/data_structures/map_helper.h>
#include <nx/fusion/model_functions.h>

namespace nx::analytics {

using namespace nx::utils::data_structures;

namespace {

using Map = MapHelper::NestedMap<std::map, QString, QString, TestDescriptor>;

static const QString kPropertyName("testDescriptors");

std::string resourceIdStr(const QnResourcePtr& resource)
{
    return resource->getId().toStdString();
};

TestDescriptor makeDescriptor(const QnResourcePtr& resource, int group, int subgroup)
{
    return TestDescriptor(
        QString("%1-%2").arg(group).arg(subgroup).toStdString(),
        resourceIdStr(resource)
        + QString("descriptor-%1-%2").arg(group).arg(subgroup).toStdString());
};

std::string uniqueDescriptorIdStr(const QnResourcePtr& resource, int group, int subgroup)
{
    return QString("%1-%2-%3")
        .arg(QString::fromStdString(resourceIdStr(resource)))
        .arg(group)
        .arg(subgroup).toStdString();
}

TestDescriptor makeUniqueDescriptor(const QnResourcePtr& resource, int group, int subgroup)
{
    return TestDescriptor(
        uniqueDescriptorIdStr(resource, group, subgroup),
        resourceIdStr(resource)
        + QString("descriptor-%1-%2").arg(group).arg(subgroup).toStdString());
};

auto makeDescriptorsForResource(const QnResourcePtr& resource)
{
    Map descriptors;
    MapHelper::set(&descriptors, "group0", "subgroup0", makeDescriptor(resource, 0, 0));
    MapHelper::set(&descriptors, "group0", "subgroup1", makeDescriptor(resource, 0, 1));
    MapHelper::set(&descriptors, "group1", "subgroup0", makeDescriptor(resource, 1, 0));
    MapHelper::set(&descriptors, "group1", "subgroup1", makeDescriptor(resource, 1, 1));
    return descriptors;
};

auto makeUniqueDescriptorsForResource(const QnResourcePtr& resource)
{
    Map descriptors;

    MapHelper::set(
        &descriptors,
        "group0",
        QString::fromStdString(uniqueDescriptorIdStr(resource, 0, 0)),
        makeUniqueDescriptor(resource, 0, 0));

    MapHelper::set(
        &descriptors,
        "group0",
        QString::fromStdString(uniqueDescriptorIdStr(resource, 0, 1)),
        makeUniqueDescriptor(resource, 0, 1));

    MapHelper::set(
        &descriptors,
        "group1",
        QString::fromStdString(uniqueDescriptorIdStr(resource, 1, 0)),
        makeUniqueDescriptor(resource, 1, 0));

    MapHelper::set(
        &descriptors,
        "group1",
        QString::fromStdString(uniqueDescriptorIdStr(resource, 1, 1)),
        makeUniqueDescriptor(resource, 1, 1));

    return descriptors;
};

} // namespace

class MultiresourceDescriptorContainerTest: public ::testing::Test
{
protected:
    using Factory = PropertyDescriptorStorageFactory<TestDescriptor, QString, QString>;
    using Container = MultiresourceDescriptorContainer<Factory, TestMergeExecutor>;

protected:
    virtual void SetUp() override
    {
        m_ownResource = makeResource();
        m_resourceList = makeResourceList();
        m_container = std::make_unique<Container>(
            Factory(kPropertyName),
            m_resourceList,
            m_ownResource);
    }

protected:
    static Map fromProperty(const QnResourcePtr& resource)
    {
        return QJson::deserialized<Map>(resource->getProperty(kPropertyName).toUtf8());
    }

    static Map makeBasicMap()
    {
        Map basicMap;
        MapHelper::set(&basicMap, "group0", "subgroup0", TestDescriptor("0-0", "descriptor-0-0"));
        MapHelper::set(&basicMap, "group0", "subgroup1", TestDescriptor("0-1", "descriptor-0-1"));
        MapHelper::set(&basicMap, "group1", "subgroup0", TestDescriptor("1-0", "descriptor-1-0"));
        MapHelper::set(&basicMap, "group1", "subgroup1", TestDescriptor("1-1", "descriptor-1-1"));
        return basicMap;
    }

private:
    static QnResourcePtr makeResource()
    {
        QnResourcePtr resource(new QnResource());
        resource->setId(QnUuid::createUuid());
        return resource;
    }

    static QnResourceList makeResourceList()
    {
        QnResourceList resourceList;
        for (auto i = 0; i < 10; ++i)
            resourceList.push_back(makeResource());

        return resourceList;
    }

protected:
    QnResourcePtr m_ownResource;
    QnResourceList m_resourceList;
    std::unique_ptr<Container> m_container;
};

TEST_F(MultiresourceDescriptorContainerTest, settingDescriptors)
{
    Map testMap = makeBasicMap();
    m_container->setDescriptors(testMap);
    ASSERT_EQ(fromProperty(m_ownResource), testMap);
    for (const auto& resource: m_resourceList)
        ASSERT_EQ(fromProperty(resource), Map());

    Map testMap2;
    MapHelper::set(&testMap2, "group0", "subgroup0", TestDescriptor("0-0", "descriptor2-0-0"));
    MapHelper::set(&testMap2, "group0", "subgroup1", TestDescriptor("0-1", "descriptor2-0-0"));
    MapHelper::set(&testMap2, "group1", "subgroup0", TestDescriptor("1-0", "descriptor2-0-0"));
    MapHelper::set(&testMap2, "group1", "subgroup1", TestDescriptor("1-1", "descriptor2-0-0"));

    m_container->setDescriptors(testMap2);
    ASSERT_EQ(fromProperty(m_ownResource), testMap2);
    for (const auto& resource: m_resourceList)
        ASSERT_EQ(fromProperty(resource), Map());
}

TEST_F(MultiresourceDescriptorContainerTest, addingDescriptors)
{
    Map testMap = makeBasicMap();
    m_container->mergeWithDescriptors(testMap);
    ASSERT_EQ(fromProperty(m_ownResource), testMap);
    for (const auto& resource: m_resourceList)
        ASSERT_EQ(fromProperty(resource), Map());

    Map testMap2;
    MapHelper::set(&testMap2, "group0", "subgroup0", TestDescriptor("0-0", "descriptor2-0-0"));
    MapHelper::set(&testMap2, "group0", "subgroup1", TestDescriptor("0-1", "descriptor2-0-0"));
    MapHelper::set(&testMap2, "group1", "subgroup0", TestDescriptor("1-0", "descriptor2-0-0"));
    MapHelper::set(&testMap2, "group1", "subgroup1", TestDescriptor("1-1", "descriptor2-0-0"));

    Map expectedResult2;
    MapHelper::set(
        &expectedResult2, "group0", "subgroup0", TestDescriptor("0-0", "descriptor2-0-0", 1));
    MapHelper::set(
        &expectedResult2, "group0", "subgroup1", TestDescriptor("0-1", "descriptor2-0-0", 1));
    MapHelper::set(
        &expectedResult2, "group1", "subgroup0", TestDescriptor("1-0", "descriptor2-0-0", 1));
    MapHelper::set(
        &expectedResult2, "group1", "subgroup1", TestDescriptor("1-1", "descriptor2-0-0", 1));

    m_container->mergeWithDescriptors(testMap2);
    ASSERT_EQ(fromProperty(m_ownResource), expectedResult2);
    for (const auto& resource: m_resourceList)
        ASSERT_EQ(fromProperty(resource), Map());

    Map testMap3;
    MapHelper::set(&testMap3, "group2", "subgroup1", TestDescriptor("2-1", "descriptor2-0-0"));
    MapHelper::set(&testMap3, "group2", "subgroup0", TestDescriptor("2-0", "descriptor2-0-0"));
    MapHelper::set(&testMap3, "group1", "subgroup1", TestDescriptor("1-1", "descriptor2-0-0"));

    Map expectedResult3 = expectedResult2;
    MapHelper::set(
        &expectedResult3, "group2", "subgroup1", TestDescriptor("2-1", "descriptor2-0-0", 0));
    MapHelper::set(
        &expectedResult3, "group2", "subgroup0", TestDescriptor("2-0", "descriptor2-0-0", 0));
    MapHelper::set(
        &expectedResult3, "group1", "subgroup1", TestDescriptor("1-1", "descriptor2-0-0", 2));

    m_container->mergeWithDescriptors(testMap3);
    ASSERT_EQ(fromProperty(m_ownResource), expectedResult3);
    for (const auto& resource: m_resourceList)
        ASSERT_EQ(fromProperty(resource), Map());
}

TEST_F(MultiresourceDescriptorContainerTest, removingDescriptors)
{
    Map testMap = makeBasicMap();
    auto init =
        [this](const auto& descriptors)
        {
            auto serialized = QString::fromUtf8(QJson::serialized(descriptors));
            m_ownResource->setProperty(kPropertyName, serialized);
            for (auto& resource: m_resourceList)
                resource->setProperty(kPropertyName, serialized);
        };

    init(testMap);
    m_container->removeDescriptors("group0");
    for (const auto& resource: m_resourceList)
        ASSERT_EQ(fromProperty(resource), testMap);

    Map expectedResult;
    MapHelper::set(
        &expectedResult, "group1", "subgroup0", TestDescriptor("1-0", "descriptor-1-0"));
    MapHelper::set(
        &expectedResult, "group1", "subgroup1", TestDescriptor("1-1", "descriptor-1-1"));
    ASSERT_EQ(fromProperty(m_ownResource), expectedResult);

    init(testMap);
    m_container->removeDescriptors("group0", "subgroup1");
    for (const auto& resource: m_resourceList)
        ASSERT_EQ(fromProperty(resource), testMap);

    Map expectedResult2;
    MapHelper::set(
        &expectedResult2, "group0", "subgroup0", TestDescriptor("0-0", "descriptor-0-0"));
    MapHelper::set(
        &expectedResult2, "group1", "subgroup0", TestDescriptor("1-0", "descriptor-1-0"));
    MapHelper::set(
        &expectedResult2, "group1", "subgroup1", TestDescriptor("1-1", "descriptor-1-1"));
    ASSERT_EQ(fromProperty(m_ownResource), expectedResult2);
}

TEST_F(MultiresourceDescriptorContainerTest, gettingDescriptors)
{
    m_ownResource->setProperty(
        kPropertyName,
        QString::fromUtf8(QJson::serialized(makeDescriptorsForResource(m_ownResource))));

    for (auto& resource : m_resourceList)
    {
        resource->setProperty(
            kPropertyName,
            QString::fromUtf8(QJson::serialized(makeDescriptorsForResource(resource))));
    }

    for (const auto& resource : m_resourceList)
    {
        ASSERT_EQ(
            makeDescriptorsForResource(resource),
            *m_container->descriptors(resource->getId()));

        ASSERT_EQ(
            makeDescriptor(resource, 0, 0),
            *m_container->descriptors(resource->getId(), "group0", "subgroup0"));
    }

    ASSERT_EQ(
        makeDescriptorsForResource(m_ownResource),
        *m_container->descriptors(m_ownResource->getId()));

    MapHelper::Wrapper<Map>::wrap<QnUuid> allDescriptors;
    for (const auto& resource : m_resourceList)
        allDescriptors[resource->getId()] = makeDescriptorsForResource(resource);

    allDescriptors[m_ownResource->getId()] = makeDescriptorsForResource(m_ownResource);
    ASSERT_EQ(allDescriptors, *m_container->descriptors());
    ASSERT_EQ(m_container->descriptors(QnUuid::createUuid()), std::nullopt);
    ASSERT_EQ(m_container->descriptors(QnUuid::createUuid(), "group3"), std::nullopt);
}

TEST_F(MultiresourceDescriptorContainerTest, gettingMergedDescriptors)
{
    auto descriptors = makeUniqueDescriptorsForResource(m_ownResource);
    auto expectedResult = descriptors;

    MapHelper::merge(&expectedResult, descriptors, ReplacementMergeExecutor<TestDescriptor>());
    m_ownResource->setProperty(kPropertyName, QString::fromUtf8(QJson::serialized(descriptors)));

    for (auto& resource: m_resourceList)
    {
        descriptors = makeUniqueDescriptorsForResource(resource);
        MapHelper::merge(&expectedResult, descriptors, ReplacementMergeExecutor<TestDescriptor>());
        resource->setProperty(kPropertyName, QString::fromUtf8(QJson::serialized(descriptors)));
    }

    ASSERT_EQ(*m_container->mergedDescriptors(), expectedResult);
}

} // namespace nx::analytics
