#ifndef QN_FRAME_SECTION_QUERYABLE_H
#define QN_FRAME_SECTION_QUERYABLE_H

#include <QtCore/QRectF>
#include <QtGui/QCursor>

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
class FrameSectionQueryable {
public:
    /**
     * Virtual destructor.
     */
    virtual ~FrameSectionQueryable() {}

    /**
     * \param region                    Region to get frame sections for, in widget coordinates.
     * \returns                         Window frame sections that intersect the given region. 
     */
    virtual Qn::WindowFrameSections windowFrameSectionsAt(const QRectF &region) const = 0;

    /**
     * \param section                   Frame section to get cursor for.
     * \returns                         Cursor to use for the given section.
     */
    virtual QCursor windowCursorAt(Qn::WindowFrameSection section) const {
        return Qn::calculateHoverCursorShape(section);
    }

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

    /**
     * Helper overload for Qt frame sections.
     *
     * \param section                   Frame section to get cursor for.
     * \returns                         Cursor to use for the given section.
     */
    QCursor windowCursorAt(Qt::WindowFrameSection section) const {
        return windowCursorAt(Qn::toQnFrameSection(section));
    }

};

#endif // QN_FRAME_SECTION_QUERYABLE_H
