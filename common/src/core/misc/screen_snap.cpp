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
    return std::all_of(values.cbegin(), values.cend(), [](const QnScreenSnap &snap) {return snap.isValid();});
}

QSet<int> QnScreenSnaps::screens() const {
    QSet<int> screens;
    for(const QnScreenSnap &snap: values)
        screens << snap.screenIndex;
    return screens;
}

QRect QnScreenSnaps::geometry(const QList<QRect> &screens) const {
    if (!isValid())
        return QRect();

    QRect geometry;
    int maxIndex = screens.size() - 1;

    auto leftCoord = [&screens, maxIndex](const QnScreenSnap &snap) {
        QRect screenRect = screens.at(qMin(snap.screenIndex, maxIndex));
        return screenRect.left() + (screenRect.width() / QnScreenSnap::snapsPerScreen()) * snap.snapIndex;
    };

    auto topCoord = [&screens, maxIndex](const QnScreenSnap &snap) {
        QRect screenRect = screens.at(qMin(snap.screenIndex, maxIndex));
        return screenRect.top() + (screenRect.height() / QnScreenSnap::snapsPerScreen()) * snap.snapIndex;
    };

    auto rightCoord = [&screens, maxIndex](const QnScreenSnap &snap) {
        QRect screenRect = screens.at(qMin(snap.screenIndex, maxIndex));
        return screenRect.right() - (screenRect.width() / QnScreenSnap::snapsPerScreen()) * snap.snapIndex;
    };

    auto bottomCoord = [&screens, maxIndex](const QnScreenSnap &snap) {
        QRect screenRect = screens.at(qMin(snap.screenIndex, maxIndex));
        return screenRect.bottom() - (screenRect.height() / QnScreenSnap::snapsPerScreen()) * snap.snapIndex;
    };

    return QRect(
        QPoint(leftCoord(left()), topCoord(top())),
        QPoint(rightCoord(right()), bottomCoord(bottom()))
        ).normalized(); //swap coordinates if screens were moved

}

