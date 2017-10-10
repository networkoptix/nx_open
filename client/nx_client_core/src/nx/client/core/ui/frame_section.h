#pragma once

#include <QtCore/QObject>
#include <QtGui/QtGui>

namespace nx {
namespace client {
namespace core {
namespace ui {

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

    static void registedQmlType();
};

} // namespace ui
} // namespace core
} // namespace client
} // namespace nx
