#ifndef QN_FRAME_SECTION_H
#define QN_FRAME_SECTION_H

#include <QtCore/Qt>
#include <QtCore/QRectF>

namespace Qn {
    /**
     * This enum is used to describe parts of a window frame. 
     * 
     * Unlike the one supplied by Qt, several values of this enum can be ORed together.
     */
    enum WindowFrameSection {
        NoSection =             0,
        LeftSection =           1 << Qt::LeftSection,
        TopLeftSection =        1 << Qt::TopLeftSection,
        TopSection =            1 << Qt::TopSection,
        TopRightSection	=       1 << Qt::TopRightSection,
        RightSection =          1 << Qt::RightSection,
        BottomRightSection =    1 << Qt::BottomRightSection,
        BottomSection =         1 << Qt::BottomSection,
        BottomLeftSection =     1 << Qt::BottomLeftSection,
        TitleBarArea =          1 << Qt::TitleBarArea,
    };

    Q_DECLARE_FLAGS(WindowFrameSections, WindowFrameSection);

    Qn::WindowFrameSection toQnFrameSection(Qt::WindowFrameSection section);

    Qt::WindowFrameSection toQtFrameSection(Qn::WindowFrameSection section);

    Qt::WindowFrameSection toNaturalQtFrameSection(Qn::WindowFrameSections sections);

    Qn::WindowFrameSections calculateRectangularFrameSections(const QRectF &frameRect, const QRectF &rect, const QRectF &query);

    Qt::CursorShape calculateHoverCursorShape(Qt::WindowFrameSection section);

    QSizeF calculateResizeDelta(const QPointF &delta, Qt::WindowFrameSection section);

    QSize calculateResizeDelta(const QPoint &delta, Qt::WindowFrameSection section);

    QRectF resizeRect(const QRectF &rect, const QSizeF &size, Qt::WindowFrameSection section);

    QRect resizeRect(const QRect &rect, const QSize &size, Qt::WindowFrameSection section);


} // namespace Qn

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::WindowFrameSections);


#endif // QN_FRAME_SECTION_H
