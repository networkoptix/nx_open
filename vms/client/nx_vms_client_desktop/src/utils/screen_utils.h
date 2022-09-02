// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QRect>
#include <QtCore/QSet>

namespace nx {
namespace gui {

class Screens: public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE static int count();

    /** List of screen geometries in physical coordinates.*/
    Q_INVOKABLE static QList<QRect> physicalGeometries();

    /** List of screen geometries in logical coordinates.*/
    Q_INVOKABLE static QList<QRect> logicalGeometries();

    static constexpr qreal kMinAreaOverlapping = 0.25;

    Q_INVOKABLE static QSet<int> coveredBy(
        const QRect& geometry,
        QList<QRect> screenGeometries,
        qreal minAreaOverlapping = kMinAreaOverlapping);

    static void registerQmlType();
};

} // namespace gui
} // namespace nx
