#pragma once

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QRect>
#include <QtCore/QSet>

struct QnScreenSnaps;

namespace nx {
namespace gui {

class Screens: public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE static int count();

    Q_INVOKABLE static QList<QRect> geometries();

    static constexpr qreal kMinAreaOverlapping = 0.25;

    Q_INVOKABLE static QSet<int> coveredBy(
        const QRect& geometry,
        QList<QRect> screenGeometries = {},
        qreal minAreaOverlapping = kMinAreaOverlapping);

    Q_INVOKABLE static QSet<int> coveredBy(
        const QnScreenSnaps& screenSnaps,
        QList<QRect> screenGeometries = {},
        qreal minAreaOverlapping = kMinAreaOverlapping);

    static void registerQmlType();
};

} // namespace gui
} // namespace nx
