// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QSet>
#include <QtCore/QPoint>

#include <nx/vms/client/core/utils/grid_walker.h>

#include <nx/utils/qset.h>
#include <utils/common/hash.h>

namespace nx::vms::client::core {

namespace {

void checkSequence(GridWalker& w, QList<QPoint> sequence)
{
    ASSERT_EQ(nx::utils::toQSet(sequence).size(), sequence.size());
    for (const auto& point: sequence)
    {
        ASSERT_TRUE(w.next());
        ASSERT_EQ(point, w.pos());
    }
    ASSERT_FALSE(w.next());
}

} // namespace

TEST(GridWalkerTest, emptyRect)
{
    const QRect r;
    GridWalker wSeq(r, GridWalker::Policy::Sequential);
    ASSERT_FALSE(wSeq.next());
    GridWalker wSn(r, GridWalker::Policy::Snake);
    ASSERT_FALSE(wSn.next());
    GridWalker wR(r, GridWalker::Policy::Round);
    ASSERT_FALSE(wR.next());
}

TEST(GridWalkerTest, singleSquare)
{
    const QRect r(0, 0, 1, 1);
    GridWalker w(r);
    ASSERT_TRUE(w.next());
    ASSERT_EQ(QPoint(0, 0), w.pos());
    ASSERT_FALSE(w.next());
}

TEST(GridWalkerTest, square3x3Sequential)
{
    const QRect r(0, 0, 3, 3);
    GridWalker w(r);
    checkSequence(w, {
        {0,0}, {1,0}, {2,0},
        {0,1}, {1,1}, {2,1},
        {0,2}, {1,2}, {2,2}});
}

TEST(GridWalkerTest, square3x3Snake)
{
    const QRect r(0, 0, 3, 3);
    GridWalker w(r, GridWalker::Policy::Snake);
    checkSequence(w, {
        {0,0}, {1,0}, {2,0},
        {2,1}, {1,1}, {0,1},
        {0,2}, {1,2}, {2,2}});
}

TEST(GridWalkerTest, negative3x3Snake)
{
    const QRect r(-1, -1, 3, 3);
    GridWalker w(r, GridWalker::Policy::Snake);
    checkSequence(w, {
        {-1,-1}, {0,-1}, {1,-1},
        {1,0}, {0,0}, {-1,0},
        {-1,1}, {0,1}, {1,1}});
}

TEST(GridWalkerTest, square2x2Round)
{
    const QRect r(0, 0, 2, 2);
    GridWalker w(r, GridWalker::Policy::Round);
    checkSequence(w, {
        {0,0}, {1,0},
        {1,1}, {0,1}});
}

TEST(GridWalkerTest, square3x3Round)
{
    const QRect r(0, 0, 3, 3);
    GridWalker w(r, GridWalker::Policy::Round);
    checkSequence(w, {
        {0,0}, {1,0}, {2,0},
        {2,1}, {2,2}, {1,2},
        {0,2}, {0,1}, {1,1}});
}

TEST(GridWalkerTest, negative3x3Round)
{
    const QRect r(-1, -1, 3, 3);
    GridWalker w(r, GridWalker::Policy::Round);
    checkSequence(w, {
        {-1,-1}, {0,-1}, {1,-1},
        {1,0}, {1,1}, {0,1},
        {-1,1}, {-1,0}, {0,0}});
}

TEST(GridWalkerTest, rect4x3Round)
{
    const QRect r(0, 0, 4, 3);
    GridWalker w(r, GridWalker::Policy::Round);
    checkSequence(w, {
        {0,0}, {1,0}, {2,0}, {3,0},
        {3,1}, {3,2}, {2,2}, {1,2},
        {0,2}, {0,1}, {1,1}, {2,1}});
}

TEST(GridWalkerTest, square4x4Round)
{
    const QRect r(0, 0, 4, 4);
    GridWalker w(r, GridWalker::Policy::Round);
    checkSequence(w, {
        {0,0}, {1,0}, {2,0}, {3,0},
        {3,1}, {3,2}, {3,3}, {2,3},
        {1,3}, {0,3}, {0,2}, {0,1},
        {1,1}, {2,1}, {2,2}, {1,2}});
}

} // namespace nx::vms::client::core
