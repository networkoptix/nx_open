#ifndef QN_TOOL_TIP_QUERYABLE_H
#define QN_TOOL_TIP_QUERYABLE_H

#include <QtCore/QString>

/**
 * This class is a workaround for a problem with total impossibility of
 * hooking into <tt>QToolTip</tt> without hacking into Qt privates. 
 * This is why if the user wants to use hand-rolled tooltips 
 * (e.g. graphics scene-based), <tt>QEvent::ToolTip</tt> is of no use to him.
 * 
 * The problem is solved by introducing a separate function that returns
 * tooltip text given a point inside an item. This function can then be
 * invoked from a global tooltip handler without sending tooltip events to the
 * items.
 */
class ToolTipQueryable {
public:
    virtual ~ToolTipQueryable() {}

    // TODO: bool toolTipAt(const QPointF &pos, QString *toolTip, QRectF *area) const
    /**
     * \param pos                       Position inside an item.
     * \returns                         Tooltip at the given position.
     */
    virtual QString toolTipAt(const QPointF &pos) const = 0;
};


#endif // QN_TOOL_TIP_QUERYABLE_H
