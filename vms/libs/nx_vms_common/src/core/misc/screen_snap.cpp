// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "screen_snap.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

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
    return std::all_of(values.cbegin(), values.cend(),
        [](const QnScreenSnap &snap)
        {
            return snap.isValid();
        });
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
