// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "screen_utils.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtQml/QtQml>

#include <nx/build_info.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/utils/geometry.h>

namespace nx {
namespace gui {

using vms::client::core::Geometry;

/**
 * TODO: #sivanov This module should be moved to nx/gui library together with Geometry class.
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

QList<QRect> Screens::physicalGeometries()
{
    QList<QRect> result;
    for (const auto screen: QGuiApplication::screens())
    {
        const auto rect = screen->geometry();
        const auto pixelRatio = screen->devicePixelRatio();
        result.append(QRect(rect.topLeft(), rect.size() * pixelRatio));
    }
    return result;
}

QList<QRect> Screens::logicalGeometries()
{
    QList<QRect> result;
    for (const auto screen: QGuiApplication::screens())
        result.append(screen->geometry());
    return result;
}

QSet<int> Screens::coveredBy(
    const QRect& geometry,
    QList<QRect> screenGeometries,
    qreal minAreaOverlapping)
{
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

void Screens::registerQmlType()
{
    qmlRegisterSingletonType<Screens>("nx.gui", 1, 0, "Screens", &createInstance);
}

} // namespace gui
} // namespace nx
