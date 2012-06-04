#include "frame_section_queryable.h"

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
    return static_cast<Qn::WindowFrameSection>(1 << (section - 1));
}

Qt::WindowFrameSection Qn::toQtFrameSection(Qn::WindowFrameSection section) {
    Qt::WindowFrameSection result = static_cast<Qt::WindowFrameSection>(qIntegerLog2(section) + 1);

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
