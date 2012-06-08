#include "frame_section.h"

#include <QtCore/QRectF>

#include <utils/common/warnings.h>
#include <utils/common/math.h>

Qt::WindowFrameSection Qn::toNaturalQtFrameSection(Qn::WindowFrameSections sections) {
    if(sections == 0) { /* Filter out the most common case first. */
        return Qt::NoSection;
    } else if(sections & (Qn::TopLeftSection | Qn::BottomRightSection | Qn::TopRightSection | Qn::BottomLeftSection)) {
        if(sections & (Qn::TopLeftSection | Qn::BottomRightSection)) {
            if(sections & Qn::BottomRightSection) {
                return Qt::BottomRightSection;
            } else {
                return Qt::TopLeftSection;
            }
        } else {
            if(sections & Qn::TopRightSection) {
                return Qt::TopRightSection;
            } else {
                return Qt::BottomLeftSection;
            }
        }
    } else if(sections & (Qn::LeftSection | Qn::RightSection | Qn::TopSection | Qn::BottomSection)) {
        if(sections & (Qn::LeftSection | Qn::RightSection)) {
            if(sections & Qn::RightSection) {
                return Qt::RightSection;
            } else {
                return Qt::LeftSection;
            }
        } else {
            if(sections & Qn::BottomSection) {
                return Qt::BottomSection;
            } else {
                return Qt::TopSection;
            }
        }
    } else if(sections & Qn::TitleBarArea) {
        return Qt::TitleBarArea;
    } else {
        qnWarning("Invalid Qn::WindowFrameSections '%1'.", static_cast<int>(sections));
        return Qt::NoSection;
    }
}

Qn::WindowFrameSection Qn::toQnFrameSection(Qt::WindowFrameSection section) {
    return static_cast<Qn::WindowFrameSection>(1 << section);
}

Qt::WindowFrameSection Qn::toQtFrameSection(Qn::WindowFrameSection section) {
    if(section == Qn::NoSection)
        return Qt::NoSection; /* qIntegerLog2's result is undefined if input is zero. */

    Qt::WindowFrameSection result = static_cast<Qt::WindowFrameSection>(qIntegerLog2(section));

    if(toQnFrameSection(result) != section) {
        qnWarning("Invalid Qn::WindowFrameSection '%1'.", static_cast<int>(section));
        return Qt::NoSection;
    }

    return result;
}

namespace {
    static const Qn::WindowFrameSection frameSectionTable[5][5] = {
        {Qn::NoSection, Qn::NoSection,          Qn::NoSection,      Qn::NoSection,          Qn::NoSection},
        {Qn::NoSection, Qn::TopLeftSection,     Qn::TopSection,     Qn::TopRightSection,    Qn::NoSection},
        {Qn::NoSection, Qn::LeftSection,        Qn::NoSection,      Qn::RightSection,       Qn::NoSection},
        {Qn::NoSection, Qn::BottomLeftSection,  Qn::BottomSection,  Qn::BottomRightSection, Qn::NoSection},
        {Qn::NoSection, Qn::NoSection,          Qn::NoSection,      Qn::NoSection,          Qn::NoSection}
    };

    inline int sweepIndex(qreal v, qreal v0, qreal v1, qreal v2, qreal v3) {
        if(v < v1) {
            if(v < v0) {
                return 0;
            } else {
                return 1;
            }
        } else {
            if(v < v2) {
                return 2;
            } else if(v < v3) {
                return 3;
            } else {
                return 4;
            }
        }
    }

} // anonymous namespace

Qn::WindowFrameSections Qn::calculateRectangularFrameSections(const QRectF &frameRect, const QRectF &rect, const QRectF &query) {
    /* Shortcuts for position. */
    qreal qx0 = query.left();
    qreal qx1 = query.right();
    qreal qy0 = query.top();
    qreal qy1 = query.bottom();

    /* Border shortcuts. */
    qreal x0 = frameRect.left();
    qreal x1 = rect.left();
    qreal x2 = rect.right();
    qreal x3 = frameRect.right();
    qreal y0 = frameRect.top();
    qreal y1 = rect.top();
    qreal y2 = rect.bottom();
    qreal y3 = frameRect.bottom();

    int cl = qMax(1, sweepIndex(qx0, x0, x1, x2, x3));
    int ch = qMin(3, sweepIndex(qx1, x0, x1, x2, x3));
    int rl = qMax(1, sweepIndex(qy0, y0, y1, y2, y3));
    int rh = qMin(3, sweepIndex(qy1, y0, y1, y2, y3));

    Qn::WindowFrameSections result = Qn::NoSection;
    for(int r = rl; r <= rh; r++)
        for(int c = cl; c <= ch; c++)
            result |= frameSectionTable[r][c];
    return result;
}

Qt::CursorShape Qn::calculateHoverCursorShape(Qt::WindowFrameSection section) {
    switch (section) {
    case Qt::TopLeftSection:
    case Qt::BottomRightSection:
        return Qt::SizeFDiagCursor;
    case Qt::TopRightSection:
    case Qt::BottomLeftSection:
        return Qt::SizeBDiagCursor;
    case Qt::LeftSection:
    case Qt::RightSection:
        return Qt::SizeHorCursor;
    case Qt::TopSection:
    case Qt::BottomSection:
        return Qt::SizeVerCursor;
    case Qt::NoSection:
    case Qt::TitleBarArea:
        return Qt::ArrowCursor;
    default:
        qnWarning("Invalid window frame section '%1'.", section);
        return Qt::ArrowCursor;
    }
}


namespace {
    template<class Rect, class Size>
    Rect resizeRectInternal(const Rect &rect, const Size &size, Qt::WindowFrameSection section) {
        /* Note that QRect::right & QRect::bottom return not what is expected (see Qt docs).
         * This is why these methods are not used here. */
        switch (section) {
        case Qt::LeftSection:
            return Rect(
                rect.left() + rect.width() - size.width(), 
                rect.top(),
                size.width(),
                rect.height()
            );
        case Qt::TopLeftSection:
            return Rect(
                rect.left() + rect.width() - size.width(), 
                rect.top() + rect.height() - size.height(),
                size.width(), 
                size.height()
            );
        case Qt::TopSection:
            return Rect(
                rect.left(), 
                rect.top() + rect.height() - size.height(),
                rect.width(), 
                size.height()
            );
        case Qt::TopRightSection:
            return Rect(
                rect.left(),
                rect.top() + rect.height() - size.height(),
                size.width(),
                size.height()
            );
        case Qt::RightSection:
            return Rect(
                rect.left(),
                rect.top(),
                size.width(),
                rect.height()
            );
        case Qt::BottomRightSection:
            return Rect(
                rect.left(),
                rect.top(),
                size.width(),
                size.height()
            );
        case Qt::BottomSection:
            return Rect(
                rect.left(),
                rect.top(),
                rect.width(),
                size.height()
            );
        case Qt::BottomLeftSection:
            return Rect(
                rect.left() + rect.width() - size.width(), 
                rect.top(),
                size.width(), 
                size.height()
            );
        case Qt::TitleBarArea:
            return Rect(
                rect.topLeft(),
                size
            );
        default:
            qnWarning("Invalid window frame section '%1'.", section);
            return rect;
        }
    }

    template<class Size, class Point>
    Size calculateSizeDeltaInternal(const Point &dragDelta, Qt::WindowFrameSection section) {
        switch (section) {
        case Qt::LeftSection:
            return Size(-dragDelta.x(), 0);
        case Qt::TopLeftSection:
            return Size(-dragDelta.x(), -dragDelta.y());
        case Qt::TopSection:
            return Size(0, -dragDelta.y());
        case Qt::TopRightSection:
            return Size(dragDelta.x(), -dragDelta.y());
        case Qt::RightSection:
            return Size(dragDelta.x(), 0);
        case Qt::BottomRightSection:
            return Size(dragDelta.x(), dragDelta.y());
        case Qt::BottomSection:
            return Size(0, dragDelta.y());
        case Qt::BottomLeftSection:
            return Size(-dragDelta.x(), dragDelta.y());
        case Qt::TitleBarArea:
            return Size(0, 0);
        default:
            qnWarning("Invalid window frame section '%1'.", section);
            return Size(0, 0);
        }
    }


    template<class Point, class Rect>
    Point calculatePinPointInternal(const Rect &rect, Qt::WindowFrameSection section) {
        /* Note that QRect::right & QRect::bottom return not what is expected (see Qt docs).
         * This is why these methods are not used here. */
        switch(section) {
        case Qt::LeftSection:
            return rect.topRight();
        case Qt::TopLeftSection:
            return rect.bottomRight();
        case Qt::TopSection:
            return rect.bottomLeft();
        case Qt::TopRightSection:
            return rect.bottomLeft();
        case Qt::RightSection:
            return rect.topLeft();
        case Qt::BottomRightSection:
            return rect.topLeft();
        case Qt::BottomSection:
            return rect.topLeft();
        case Qt::BottomLeftSection:
            return rect.topRight();
        case Qt::TitleBarArea:
            qnWarning("There is no pin-point when dragging title bar area.");
            return Point();
        default:
            qnWarning("Invalid window frame section '%1'.", section);
            return Point();
        }
    }

} // anonymous namespace

QSizeF Qn::calculateSizeDelta(const QPointF &dragDelta, Qt::WindowFrameSection section) {
    return calculateSizeDeltaInternal<QSizeF, QPointF>(dragDelta, section);
}

QSize Qn::calculateSizeDelta(const QPoint &dragDelta, Qt::WindowFrameSection section) {
    return calculateSizeDeltaInternal<QSize, QPoint>(dragDelta, section);
}

QRectF Qn::resizeRect(const QRectF &rect, const QSizeF &size, Qt::WindowFrameSection section) {
    return resizeRectInternal<QRectF, QSizeF>(rect, size, section);
}

QRect Qn::resizeRect(const QRect &rect, const QSize &size, Qt::WindowFrameSection section) {
    return resizeRectInternal<QRect, QSize>(rect, size, section);
}

QPointF Qn::calculatePinPoint(const QRectF &rect, Qt::WindowFrameSection section) {
    return calculatePinPointInternal<QPointF, QRectF>(rect, section);
}

QPoint Qn::calculatePinPoint(const QRect &rect, Qt::WindowFrameSection section) {
    return calculatePinPointInternal<QPoint, QRect>(rect, section);
}
