#pragma once

#include <QtCore/QObject>
#include <QtGui/QtGui>

namespace nx {
namespace client {
namespace core {

class FrameSection: public QObject
{
    Q_OBJECT

public:
    enum Section
    {
        NoSection = 0,
        Left = 0x01,
        Right = 0x02,
        Top = 0x04,
        Bottom = 0x08,
        TopLeft = Top | Left,
        TopRight = Top | Right,
        BottomLeft = Bottom | Left,
        BottomRight = Bottom | Right
    };
    Q_ENUM(Section)
    Q_DECLARE_FLAGS(SectionFlags, Section)

    Q_INVOKABLE static Section frameSection(
        const QPointF& point,
        const QRectF& rect,
        qreal frameWidth);

    Q_INVOKABLE static bool isEdge(Section section);
    Q_INVOKABLE static bool isCorner(Section section);

    Q_INVOKABLE static Qt::WindowFrameSection toQtWindowFrameSection(Section section);

    Q_INVOKABLE static QPointF sectionCenterPoint(const QRectF& rect, Section section);
    Q_INVOKABLE static Section oppositeSection(Section section);

    Q_INVOKABLE static QSizeF sizeDelta(const QPointF& dragDelta, Section section);

    static void registedQmlType();
};

} // namespace core
} // namespace client
} // namespace nx
