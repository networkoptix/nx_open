#ifndef QN_FRAME_SECTION_H
#define QN_FRAME_SECTION_H

#include <QtCore/Qt>
#include <QtCore/QRectF>

#include <common/common_globals.h>

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
        TopRightSection =       1 << Qt::TopRightSection,
        RightSection =          1 << Qt::RightSection,
        BottomRightSection =    1 << Qt::BottomRightSection,
        BottomSection =         1 << Qt::BottomSection,
        BottomLeftSection =     1 << Qt::BottomLeftSection,
        TitleBarArea =          1 << Qt::TitleBarArea,

        CornerSections =        TopLeftSection | TopRightSection | BottomLeftSection | BottomRightSection,
        SideSections =          TopSection | BottomSection | LeftSection | RightSection,
        ResizeSections =        CornerSections | SideSections,
    };

    Q_DECLARE_FLAGS(WindowFrameSections, WindowFrameSection);

    Qn::WindowFrameSection toQnFrameSection(Qt::WindowFrameSection section);

    Qt::WindowFrameSection toQtFrameSection(Qn::WindowFrameSection section);

    Qt::WindowFrameSection toNaturalQtFrameSection(Qn::WindowFrameSections sections);

    Qn::WindowFrameSections calculateRectangularFrameSections(const QRectF &frameRect, const QRectF &rect, const QRectF &query);

    Qn::WindowFrameSections calculateRectangularFrameSections(const QRect &frameRect, const QRect &rect, const QRect &query);

    Qt::CursorShape calculateHoverCursorShape(Qn::WindowFrameSection section);
    
    Qt::CursorShape calculateHoverCursorShape(Qt::WindowFrameSection section);


    QSizeF calculateSizeDelta(const QPointF &dragDelta, Qt::WindowFrameSection section);

    QSize calculateSizeDelta(const QPoint &dragDelta, Qt::WindowFrameSection section);

    QPointF calculatePinPoint(const QRectF &rect, Qt::WindowFrameSection section);

    QPoint calculatePinPoint(const QRect &rect, Qt::WindowFrameSection section);

    Qn::Corner calculatePinPoint(Qt::WindowFrameSection section);


    QRectF resizeRect(const QRectF &rect, const QSizeF &size, Qt::WindowFrameSection section);

    QRect resizeRect(const QRect &rect, const QSize &size, Qt::WindowFrameSection section);

    Qt::WindowFrameSection rotateSection(Qt::WindowFrameSection section, qreal rotation);

} // namespace Qn

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::WindowFrameSections);


#endif // QN_FRAME_SECTION_H
