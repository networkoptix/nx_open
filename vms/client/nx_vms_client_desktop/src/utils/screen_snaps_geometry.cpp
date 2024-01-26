// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "screen_snaps_geometry.h"

#include <nx/utils/log/log.h>

QRect screenSnapsGeometry(const QnScreenSnaps& snaps, const QList<QRect>& screenGeometries)
{
    if (!snaps.isValid())
        return {};

    if (!NX_ASSERT(!screenGeometries.empty()))
        return {};

    auto geometryByScreenIndex =
        [&screenGeometries](int i)
        {
            return i >= 0 && i < screenGeometries.size()
                ? screenGeometries[i]
                : screenGeometries[0];
        };

    const QRect leftScreen = geometryByScreenIndex(snaps.left().screenIndex);
    const int x1 = leftScreen.x()
        + (leftScreen.width() / QnScreenSnap::snapsPerScreen()) * snaps.left().snapIndex;

    const QRect topScreen = geometryByScreenIndex(snaps.top().screenIndex);
    const int y1 = topScreen.y()
        + (topScreen.height() / QnScreenSnap::snapsPerScreen()) * snaps.top().snapIndex;

    const QRect rightScreen = geometryByScreenIndex(snaps.right().screenIndex);
    const int x2 = rightScreen.right()
        - (rightScreen.width() / QnScreenSnap::snapsPerScreen()) * snaps.right().snapIndex;

    const QRect bottomScreen = geometryByScreenIndex(snaps.bottom().screenIndex);
    const int y2 = bottomScreen.bottom()
        - (bottomScreen.height() / QnScreenSnap::snapsPerScreen()) * snaps.bottom().snapIndex;

    // Swap coordinates if screens were moved.
    return QRect(QPoint(x1, y1), QPoint(x2, y2)).normalized();
}

QRect screenSnapsGeometryScreenRelative(
    const QnScreenSnaps& snaps,
    const QList<QRect>& physicalScreenGeometries,
    QScreen* screen)
{
    if (!snaps.isValid())
        return {};

    if (!NX_ASSERT(!physicalScreenGeometries.empty()))
        return {};

    const auto screenGeometry = screen->geometry();
    const auto screenPixelRatio = screen->devicePixelRatio();

    auto screenByIndex =
        [&physicalScreenGeometries](int i)
        {
            return i >= 0 && i < physicalScreenGeometries.size()
                ? physicalScreenGeometries[i]
                : physicalScreenGeometries[0];
        };

    const QRect leftScreen = screenByIndex(snaps.left().screenIndex);
    const int x1 = screenGeometry.x()
        + (int)((leftScreen.left() - screenGeometry.left() + (int)(leftScreen.width() / QnScreenSnap::snapsPerScreen()) * snaps.left().snapIndex) / screenPixelRatio);

    const QRect topScreen = screenByIndex(snaps.top().screenIndex);
    const int y1 = screenGeometry.y()
        + (int)((topScreen.top() - screenGeometry.top() + (int)(topScreen.height() / QnScreenSnap::snapsPerScreen()) * snaps.top().snapIndex) / screenPixelRatio);

    const QRect rightScreen = screenByIndex(snaps.right().screenIndex);
    const int x2 = screenGeometry.x()
        + (int)((rightScreen.right() - screenGeometry.left() - (int)(rightScreen.width() / QnScreenSnap::snapsPerScreen()) * snaps.right().snapIndex) / screenPixelRatio);

    const QRect bottomScreen = screenByIndex(snaps.bottom().screenIndex);
    const int y2 = screenGeometry.y()
        + (int)((bottomScreen.bottom() - screenGeometry.top() - (int)(bottomScreen.height() / QnScreenSnap::snapsPerScreen()) * snaps.bottom().snapIndex) / screenPixelRatio);

    // Swap coordinates if screens were moved.
    return QRect(QPoint(x1, y1), QPoint(x2, y2)).normalized();
}
