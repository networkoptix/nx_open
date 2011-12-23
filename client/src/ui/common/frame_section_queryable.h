#ifndef QN_FRAME_SECTION_QUERYABLE_H
#define QN_FRAME_SECTION_QUERYABLE_H

#include <QtGlobal>
#include <utils/common/warnings.h>

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

    inline Qn::WindowFrameSection toQnFrameSection(Qt::WindowFrameSection section) {
        return static_cast<Qn::WindowFrameSection>(1 << (section - 1));
    }

    inline Qt::WindowFrameSection toQtFrameSection(Qn::WindowFrameSection section) {
        switch(section) {
        case NoSection:             return Qt::NoSection;
        case LeftSection:           return Qt::LeftSection;
        case TopLeftSection:        return Qt::TopLeftSection;
        case TopSection:            return Qt::TopSection;
        case TopRightSection:       return Qt::TopRightSection;
        case RightSection:          return Qt::RightSection;
        case BottomRightSection:    return Qt::BottomRightSection;
        case BottomSection:         return Qt::BottomSection;
        case BottomLeftSection:     return Qt::BottomLeftSection;
        case TitleBarArea:          return Qt::TitleBarArea;
        default:
            qnWarning("Invalid Qn::WindowFrameSection '%1'.", static_cast<int>(section));
            return Qt::NoSection;
        }
    }

    inline Qt::WindowFrameSection toNaturalQtFrameSection(Qn::WindowFrameSections sections) {
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

} // namespace Qn

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::WindowFrameSections);

/**
 * This class is a workaround for pixel-hunting problems when resizing zoomed-out
 * graphics widgets.
 * 
 * Default frame section detection algorithm works on point level, so when zoomed
 * out it becomes really difficult (and sometimes even impossible) to hit the
 * resizing grip area with a mouse.
 * 
 * This problem is solved by introducing a separate function that returns a set
 * of all window frame sections that intersect the given rectangle.
 */
class FrameSectionQuearyable {
public:
    /**
     * \param region                    Region to get frame sections for.
     * \returns                         Window frame sections that intersect the given region. 
     */
    virtual Qn::WindowFrameSections windowFrameSectionsAt(const QRectF &region) const = 0;

    /**
     * This function calculates frame section with the highest priority that
     * intersects the given region. Section are prioritized in "natural" order,
     * e.g. corner sections are prioritized over side ones.
     *
     * \param region                    Region to get prioritized section for.
     * \returns                         Window frame section with highest priority that intersects the given region.
     */
    Qt::WindowFrameSection windowFrameSectionAt(const QRectF &region) const {
        return toNaturalQtFrameSection(windowFrameSectionsAt(region));
    }
};

#endif // QN_FRAME_SECTION_QUERYABLE_H
