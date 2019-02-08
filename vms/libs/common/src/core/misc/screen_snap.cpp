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

QRect QnScreenSnaps::geometry(const QList<QRect>& screenGeometries) const
{
    if (!isValid())
        return {};

    QRect geometry;
    int maxIndex = screenGeometries.size() - 1;

    auto leftCoord =
        [&screenGeometries, maxIndex](const QnScreenSnap& snap)
        {
            QRect screenRect = screenGeometries.at(qMin(snap.screenIndex, maxIndex));
            return screenRect.left() + (screenRect.width() / QnScreenSnap::snapsPerScreen()) * snap.snapIndex;
        };

    auto topCoord =
        [&screenGeometries, maxIndex](const QnScreenSnap& snap)
        {
            QRect screenRect = screenGeometries.at(qMin(snap.screenIndex, maxIndex));
            return screenRect.top() + (screenRect.height() / QnScreenSnap::snapsPerScreen()) * snap.snapIndex;
        };

    auto rightCoord =
        [&screenGeometries, maxIndex](const QnScreenSnap& snap)
        {
            QRect screenRect = screenGeometries.at(qMin(snap.screenIndex, maxIndex));
            return screenRect.right() - (screenRect.width() / QnScreenSnap::snapsPerScreen()) * snap.snapIndex;
        };

    auto bottomCoord =
        [&screenGeometries, maxIndex](const QnScreenSnap& snap)
        {
            QRect screenRect = screenGeometries.at(qMin(snap.screenIndex, maxIndex));
            return screenRect.bottom() - (screenRect.height() / QnScreenSnap::snapsPerScreen()) * snap.snapIndex;
        };

    return QRect(
        QPoint(leftCoord(left()), topCoord(top())),
        QPoint(rightCoord(right()), bottomCoord(bottom()))
    ).normalized(); //swap coordinates if screens were moved

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

