#include "frame_section.h"

#include <QtCore/QRectF>

#include <utils/common/warnings.h>
#include <utils/math/math.h>

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
    if(section == Qt::NoSection)
        return Qn::NoSection;
    
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
    const Qn::WindowFrameSection frameSectionTable[5][5] = {
        {Qn::NoSection, Qn::NoSection,          Qn::NoSection,      Qn::NoSection,          Qn::NoSection},
        {Qn::NoSection, Qn::TopLeftSection,     Qn::TopSection,     Qn::TopRightSection,    Qn::NoSection},
        {Qn::NoSection, Qn::LeftSection,        Qn::NoSection,      Qn::RightSection,       Qn::NoSection},
        {Qn::NoSection, Qn::BottomLeftSection,  Qn::BottomSection,  Qn::BottomRightSection, Qn::NoSection},
        {Qn::NoSection, Qn::NoSection,          Qn::NoSection,      Qn::NoSection,          Qn::NoSection}
    };

    template<class T>
    inline int sweepIndex(T v, T v0, T v1, T v2, T v3) {
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

    template<class T, class Rect>
    Qn::WindowFrameSections calculateRectangularFrameSectionsInternal(const Rect &frameRect, const Rect &rect, const Rect &query) {
        /* Note that QRect::right & QRect::bottom return not what is expected (see Qt docs).
         * This is why these methods are not used here. */

        /* Shortcuts for position. */
        T qx0 = query.left();
        T qx1 = query.left() + query.width();
        T qy0 = query.top();
        T qy1 = query.top() + query.height();

        /* Border shortcuts. */
        T x0 = frameRect.left();
        T x1 = rect.left();
        T x2 = rect.left() + rect.width();
        T x3 = frameRect.left() + frameRect.width();
        T y0 = frameRect.top();
        T y1 = rect.top();
        T y2 = rect.top() + rect.height();
        T y3 = frameRect.top() + frameRect.height();

        int cl = qMax(1, sweepIndex<T>(qx0, x0, x1, x2, x3));
        int ch = qMin(3, sweepIndex<T>(qx1, x0, x1, x2, x3));
        int rl = qMax(1, sweepIndex<T>(qy0, y0, y1, y2, y3));
        int rh = qMin(3, sweepIndex<T>(qy1, y0, y1, y2, y3));

        Qn::WindowFrameSections result = Qn::NoSection;
        for(int r = rl; r <= rh; r++)
            for(int c = cl; c <= ch; c++)
                result |= frameSectionTable[r][c];
        return result;
    }

} // anonymous namespace

Qn::WindowFrameSections Qn::calculateRectangularFrameSections(const QRectF &frameRect, const QRectF &rect, const QRectF &query) {
    return calculateRectangularFrameSectionsInternal<qreal, QRectF>(frameRect, rect, query);
}

Qn::WindowFrameSections Qn::calculateRectangularFrameSections(const QRect &frameRect, const QRect &rect, const QRect &query) {
    return calculateRectangularFrameSectionsInternal<int, QRect>(frameRect, rect, query);
}

Qt::CursorShape Qn::calculateHoverCursorShape(Qn::WindowFrameSection section) {
    switch (section) {
    case Qn::TopLeftSection:
    case Qn::BottomRightSection:
        return Qt::SizeFDiagCursor;
    case Qn::TopRightSection:
    case Qn::BottomLeftSection:
        return Qt::SizeBDiagCursor;
    case Qn::LeftSection:
    case Qn::RightSection:
        return Qt::SizeHorCursor;
    case Qn::TopSection:
    case Qn::BottomSection:
        return Qt::SizeVerCursor;
    case Qn::NoSection:
    case Qn::TitleBarArea:
        return Qt::ArrowCursor;
    default:
        qnWarning("Invalid window frame section '%1'.", section);
        return Qt::ArrowCursor;
    }
}

Qt::CursorShape Qn::calculateHoverCursorShape(Qt::WindowFrameSection section) {
    return calculateHoverCursorShape(toQnFrameSection(section));
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
         * This is why these methods are not used here. */ // TODO: #Elric comment says they are not used, but they are used!!!
        switch(section) {
        case Qt::LeftSection:
            return Point(rect.right(), rect.top() + rect.height() / 2);
        case Qt::TopLeftSection:
            return rect.bottomRight();
        case Qt::TopSection:
            return Point(rect.left() + rect.width() / 2, rect.bottom());
        case Qt::TopRightSection:
            return rect.bottomLeft();
        case Qt::RightSection:
            return Point(rect.left(), rect.top() + rect.height() / 2);
        case Qt::BottomRightSection:
            return rect.topLeft();
        case Qt::BottomSection:
            return Point(rect.left() + rect.width() / 2, rect.top());
        case Qt::BottomLeftSection:
            return rect.topRight();
        case Qt::TitleBarArea:
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

Qn::Corner Qn::calculatePinPoint(Qt::WindowFrameSection section) {
    switch(section) {
    case Qt::LeftSection:
        return Qn::TopRightCorner;
    case Qt::TopLeftSection:
        return Qn::BottomRightCorner;
    case Qt::TopSection:
        return Qn::BottomLeftCorner;
    case Qt::TopRightSection:
        return Qn::BottomLeftCorner;
    case Qt::RightSection:
        return Qn::TopLeftCorner;
    case Qt::BottomRightSection:
        return Qn::TopLeftCorner;
    case Qt::BottomSection:
        return Qn::TopLeftCorner;
    case Qt::BottomLeftSection:
        return Qn::TopRightCorner;
    case Qt::TitleBarArea:
        return Qn::NoCorner;
    default:
        qnWarning("Invalid window frame section '%1'.", section);
        return Qn::NoCorner;
    }
}

QPoint Qn::calculatePinPoint(const QRect &rect, Qt::WindowFrameSection section) {
    return calculatePinPointInternal<QPoint, QRect>(rect, section);
}

Qt::WindowFrameSection Qn::rotateSection(Qt::WindowFrameSection section, qreal rotation) {
    if (section == Qt::NoSection || section == Qt::TitleBarArea)
        return section;

    int intRotation = std::round(rotation); /* we don't need qreal precision */

    if (intRotation < 0)
        intRotation += 360;
    else if (intRotation >= 360)
        intRotation -= 360;

    int n45 = std::floor((intRotation + 45.0 / 2.0) / 45.0);

    static const QList<Qt::WindowFrameSection> sections = QList<Qt::WindowFrameSection>()
                                                          << Qt::LeftSection
                                                          << Qt::TopLeftSection
                                                          << Qt::TopSection
                                                          << Qt::TopRightSection
                                                          << Qt::RightSection
                                                          << Qt::BottomRightSection
                                                          << Qt::BottomSection
                                                          << Qt::BottomLeftSection;

    int index = sections.indexOf(section);
    assert(index >= 0);
    index = (index + n45) % sections.size();
    return sections[index];
}
