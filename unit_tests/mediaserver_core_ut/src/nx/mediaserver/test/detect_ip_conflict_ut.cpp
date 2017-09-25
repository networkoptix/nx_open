#include <gtest/gtest.h>

#include <core/resource_management/mserver_resource_discovery_manager.h>
#include <plugins/resource/axis/axis_onvif_resource.h>

namespace nx {
namespace mediaserver {
namespace test {

struct TestData
{
    TestData() {}
    TestData(
        const QString& physicalId, 
        const QString& url, 
        const QString& groupId = QString(),
        bool isManuallyAdded = false)
        :
        physicalId(physicalId),
        url(url),
        groupId(groupId),
        isManuallyAdded(isManuallyAdded)
    {
    }

    QString physicalId;
    QString url;
    QString groupId;
    bool isManuallyAdded = false;
};

template <std::size_t size>
bool checkData(const std::array<TestData, size>& data)
{
    QSet<QnNetworkResourcePtr> resList;
    for (const auto& value: data)
    {
        QnAxisOnvifResourcePtr res(new QnAxisOnvifResource());
        res->setPhysicalId(value.physicalId);
        res->setId(res->physicalIdToId(value.physicalId));
        res->setUrl(value.url);
        res->setGroupId(value.groupId);
        res->setManuallyAdded(value.isManuallyAdded);
        resList << res;
    }
    return QnMServerResourceDiscoveryManager::hasIpConflict(resList);
}

TEST(CameraIpConflict, main)
{
    static std::array<TestData, 2> realConflict1 =
    {
        TestData("id1", "http://192.168.1.100"),
        TestData("id2", "http://192.168.1.100")
    };
    ASSERT_TRUE(checkData(realConflict1));

    static std::array<TestData, 2> faceConflict2 =
    {
        TestData("id1", "http://192.168.1.100:8081", QString(), true),
        TestData("id2", "http://192.168.1.100:8082", QString(), true)
    };
    ASSERT_FALSE(checkData(faceConflict2));

    static std::array<TestData, 3> realConflict3 =
    {
        TestData("id1", "http://192.168.1.100:8081", QString()),
        TestData("id2", "http://192.168.1.100:8082", QString()),
        TestData("id3", "http://192.168.1.100:8081", QString())
    };
    ASSERT_TRUE(checkData(realConflict3));

    static std::array<TestData, 3> fakeConflict4 =
    {
        TestData("id1", "http://192.168.1.100", "group1"),
        TestData("id2", "http://192.168.1.100", "group1"),
        TestData("id3", "http://192.168.1.100", "group1")
    };
    ASSERT_FALSE(checkData(fakeConflict4));

    static std::array<TestData, 3> realConflict5 =
    {
        TestData("id1", "http://192.168.1.100", "group1"),
        TestData("id2", "http://192.168.1.100", "group1"),
        TestData("id3", "http://192.168.1.100", "group2")
    };
    ASSERT_TRUE(checkData(realConflict5));

    static std::array<TestData, 3> fakeConflict6 =
    {
        TestData("id1", "http://192.168.1.100", "group1", true),
        TestData("id2", "http://192.168.1.100", "group1", true),
        TestData("id3", "http://192.168.1.100", "group1", true)
    };
    ASSERT_FALSE(checkData(fakeConflict6));
}

} // namespace test
} // namespace mediaserver
} // namespace nx
