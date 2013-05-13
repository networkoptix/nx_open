#ifndef QN_FRAME_SECTION_QUERYABLE_H
#define QN_FRAME_SECTION_QUERYABLE_H

#include <QtCore/QRectF>

#include "frame_section.h"

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
     * Virtual destructor.
     */
    virtual ~FrameSectionQuearyable() {}

    /**
     * \param region                    Region to get frame sections for, in widget coordinates.
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
