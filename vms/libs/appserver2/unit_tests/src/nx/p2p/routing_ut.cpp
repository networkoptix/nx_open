#include <gtest/gtest.h>

#include <nx/p2p/routing_helpers.h>

namespace nx {
namespace p2p {
namespace test {

vms::api::PersistentIdData genId()
{
    return vms::api::PersistentIdData(QnUuid::createUuid(), QnUuid::createUuid());
}

TEST(P2pRouting, distanceTo)
{
    BidirectionRoutingInfo routing(genId());

    vms::api::PersistentIdData via1 = genId();
    vms::api::PersistentIdData to1 = genId();
    vms::api::PersistentIdData to2 = genId();
    vms::api::PersistentIdData to3 = to2;
    to3.persistentId = QnUuid::createUuid();

    routing.addRecord(via1, to1, RoutingRecord(1, via1));
    routing.addRecord(via1, to2, RoutingRecord(2, via1));
    routing.addRecord(via1, to3, RoutingRecord(3, via1));

    vms::api::PersistentIdData via2 = genId();
    routing.addRecord(via2, to1, RoutingRecord(4, via2));
    routing.addRecord(via2, to2, RoutingRecord(5, via2));
    routing.addRecord(via2, to3, RoutingRecord(6, via2));

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

TEST(P2pRouting, routeTo)
{
    BidirectionRoutingInfo routing(genId());

    vms::api::PersistentIdData via1 = genId();
    vms::api::PersistentIdData to1 = genId();
    vms::api::PersistentIdData to2 = genId();
    vms::api::PersistentIdData to3 = to2;
    to3.persistentId = QnUuid::createUuid();

    routing.addRecord(via1, to1, RoutingRecord(1, via1));
    routing.addRecord(via1, to2, RoutingRecord(2, via1));
    routing.addRecord(via1, to3, RoutingRecord(3, via1));

    vms::api::PersistentIdData via2 = genId();
    routing.addRecord(via2, to1, RoutingRecord(4, via2));
    routing.addRecord(via2, to2, RoutingRecord(5, via2));
    routing.addRecord(via2, to3, RoutingRecord(6, via2));

    RoutingInfo viaList;
    qint32 distance = routing.distanceTo(to3.id, &viaList);
    ASSERT_EQ(1, viaList.size());
    ASSERT_EQ(2, distance);

    viaList.clear();
    distance = routing.distanceTo(to3, &viaList);
    ASSERT_EQ(1, viaList.size());
    ASSERT_EQ(3, distance);
}

} // namespace test
} // namespace p2p
} // namespace nx
