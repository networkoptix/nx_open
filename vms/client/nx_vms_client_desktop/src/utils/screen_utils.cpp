#include "screen_utils.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#include <QtQml/QtQml>

#include <core/misc/screen_snap.h>

#include <nx/client/core/utils/geometry.h>

#include <nx/utils/app_info.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace gui {

using vms::client::core::Geometry;

/**
 * TODO: #GDM This module should be moved to nx/gui library together with Geometry class.
 */
namespace {

static QObject* createInstance(QQmlEngine* /*engine*/, QJSEngine* /*jsEngine*/)
{
    return new Screens();
}

} // namespace

int Screens::count()
{
    return QGuiApplication::screens().size();
}

QList<QRect> Screens::geometries()
{
    QList<QRect> result;

    if (nx::utils::AppInfo::isMacOsX())
    {
        for (const auto screen: QGuiApplication::screens())
            result.append(screen->geometry());
    }
    else
    {
        for (const auto screen: QGuiApplication::screens())
        {
            const auto rect = screen->geometry();
            const auto pixelRatio = screen->devicePixelRatio();
            result.append(QRect(rect.topLeft(), rect.size() * pixelRatio));
        }
    }
    return result;
}

QSet<int> Screens::coveredBy(
    const QRect& geometry,
    QList<QRect> screenGeometries,
    qreal minAreaOverlapping)
{
    if (screenGeometries.empty())
        screenGeometries = geometries();

    NX_ASSERT(!screenGeometries.empty());
    if (screenGeometries.empty())
        return {};

    QSet<int> result;
    for (int i = 0; i < screenGeometries.size(); ++i)
    {
        QRect screen = screenGeometries[i];
        NX_ASSERT(screen.isValid());
        if (!screen.isValid())
            continue;
        const QRect intersected = geometry.intersected(screen);
        const auto overlapping = 1.0 * Geometry::area(intersected) / Geometry::area(screen);
        if (overlapping >= minAreaOverlapping)
            result.insert(i);
    }
    return result;
}

QSet<int> Screens::coveredBy(
    const QnScreenSnaps& screenSnaps,
    QList<QRect> screenGeometries,
    qreal minAreaOverlapping)
{
    if (screenGeometries.empty())
        screenGeometries = geometries();

    NX_ASSERT(!screenGeometries.empty());
    if (screenGeometries.empty())
        return {};

    const int maxIndex = screenGeometries.size() - 1;

    QRect combinedRect;
    for (const QnScreenSnap& snap: screenSnaps.values)
    {
        const int index = qBound(0, snap.screenIndex, maxIndex);
        combinedRect = combinedRect.united(screenGeometries[index]);
    }

    return coveredBy(combinedRect, screenGeometries, minAreaOverlapping);
}

void Screens::registerQmlType()
{
    qmlRegisterSingletonType<Screens>("nx.gui", 1, 0, "Screens", &createInstance);
}


} // namespace gui
} // namespace nx
