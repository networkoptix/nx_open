#include <gtest/gtest.h>

#include <nx/p2p/routing_helpers.h>

namespace nx {
namespace p2p {
namespace test {

ApiPersistentIdData genId()
{
    return ApiPersistentIdData(QnUuid::createUuid(), QnUuid::createUuid());
}

TEST(P2pRouting, distanceTo)
{
    BidirectionRoutingInfo routing(genId());

    ec2::ApiPersistentIdData via1 = genId();
    ec2::ApiPersistentIdData to1 = genId();
    ec2::ApiPersistentIdData to2 = genId();
    ec2::ApiPersistentIdData to3 = to2;
    to3.persistentId = QnUuid::createUuid();

    routing.addRecord(via1, to1, RoutingRecord(1, 0));
    routing.addRecord(via1, to2, RoutingRecord(2, 0));
    routing.addRecord(via1, to3, RoutingRecord(3, 0));

    ec2::ApiPersistentIdData via2 = genId();
    routing.addRecord(via2, to1, RoutingRecord(4, 0));
    routing.addRecord(via2, to2, RoutingRecord(5, 0));
    routing.addRecord(via2, to3, RoutingRecord(6, 0));

    ASSERT_EQ(1, routing.distanceTo(to1));
    ASSERT_EQ(2, routing.distanceTo(to2));
    ASSERT_EQ(3, routing.distanceTo(to3));

    ASSERT_EQ(1, routing.distanceTo(to1.id));
    ASSERT_EQ(2, routing.distanceTo(to2.id));
    ASSERT_EQ(2, routing.distanceTo(to3.id));

    routing.removePeer(via1);

    ASSERT_EQ(4, routing.distanceTo(to1));
    ASSERT_EQ(5, routing.distanceTo(to2));
    ASSERT_EQ(6, routing.distanceTo(to3));

    ASSERT_EQ(4, routing.distanceTo(to1.id));
    ASSERT_EQ(5, routing.distanceTo(to2.id));
    ASSERT_EQ(5, routing.distanceTo(to3.id));

    routing.removePeer(via2);
    ASSERT_EQ(kMaxDistance, routing.distanceTo(to2));
}

} // namespace test
} // namespace p2p
} // namespace nx
