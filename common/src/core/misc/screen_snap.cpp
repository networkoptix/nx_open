#include "screen_snap.h"

namespace {
    const int snapsPerScreenValue = 2;
}

QnScreenSnap::QnScreenSnap() : 
    screenIndex(-1),
    snapIndex(0)
{
}

QnScreenSnap::QnScreenSnap(int screenIndex, int snapIndex) :
    screenIndex(screenIndex), 
    snapIndex(snapIndex)
{
}

bool QnScreenSnap::isValid() const {
    return screenIndex >= 0 && snapIndex >= 0 && snapIndex < snapsPerScreenValue;
}

int QnScreenSnap::encode() const {
    return (screenIndex << 8) | (snapIndex & 0xFF);
}

QnScreenSnap QnScreenSnap::decode(int encoded) {
    return QnScreenSnap(encoded >> 8, encoded & 0xFF);
}

int QnScreenSnap::snapsPerScreen() {
    return snapsPerScreenValue;
}

bool QnScreenSnaps::isValid() const {
    return left.isValid() && right.isValid() && top.isValid() && bottom.isValid();
}
