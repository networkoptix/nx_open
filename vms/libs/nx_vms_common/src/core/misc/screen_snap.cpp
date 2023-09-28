// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "screen_snap.h"

#include <nx/utils/log/assert.h>

namespace {

const int snapsPerScreenValue = 2;

} // namespace

QnScreenSnap::QnScreenSnap()
{
}

QnScreenSnap::QnScreenSnap(int screenIndex, int snapIndex):
    screenIndex(screenIndex),
    snapIndex(snapIndex)
{
}

bool QnScreenSnap::isValid() const
{
    return screenIndex >= 0 && snapIndex >= 0 && snapIndex < snapsPerScreenValue;
}

int QnScreenSnap::encode() const
{
    return (screenIndex << 8) | (snapIndex & 0xFF);
}

QnScreenSnap QnScreenSnap::decode(int encoded)
{
    return {encoded >> 8, encoded & 0xFF};
}

int QnScreenSnap::snapsPerScreen()
{
    return snapsPerScreenValue;
}

bool QnScreenSnaps::isValid() const
{
    return std::all_of(values.cbegin(), values.cend(), [](const QnScreenSnap &snap) { return snap.isValid(); });
}

QRect QnScreenSnaps::geometry(const QList<QRect>& screens) const
{
    if (!isValid())
        return {};

    if (!NX_ASSERT(!screens.empty()))
        return {};

    auto screen =
        [&screens](int i) { return i >= 0 && i < screens.size() ? screens[i] : screens[0]; };

    const QRect leftScreen = screen(left().screenIndex);
    const int x1 = leftScreen.x()
        + (leftScreen.width() / QnScreenSnap::snapsPerScreen()) * left().snapIndex;

    const QRect topScreen = screen(top().screenIndex);
    const int y1 = topScreen.y()
        + (topScreen.height() / QnScreenSnap::snapsPerScreen()) * top().snapIndex;

    const QRect rightScreen = screen(right().screenIndex);
    const int x2 = rightScreen.right()
        - (rightScreen.width() / QnScreenSnap::snapsPerScreen()) * right().snapIndex;

    const QRect bottomScreen = screen(bottom().screenIndex);
    const int y2 = bottomScreen.bottom()
        - (bottomScreen.height() / QnScreenSnap::snapsPerScreen()) * bottom().snapIndex;

    // Swap coordinates if screens were moved.
    return QRect(QPoint(x1, y1), QPoint(x2, y2)).normalized();
}

QDebug operator<<(QDebug dbg, const QnScreenSnap& snap)
{
    dbg.nospace() << "[" << snap.screenIndex << ":" << snap.snapIndex << "]";
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const QnScreenSnaps& snaps)
{
    dbg.nospace() << "QnScreenSnaps("
        << snaps.left()
        << snaps.right()
        << snaps.top()
        << snaps.bottom()
        << ")";
    return dbg.space();
}
