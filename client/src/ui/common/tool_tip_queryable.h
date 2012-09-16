#ifndef QN_TOOL_TIP_QUERYABLE_H
#define QN_TOOL_TIP_QUERYABLE_H

#include <QtCore/QString>

class QnToolTipQueryable {
public:
    virtual QString toolTipAt(const QPointF &pos) const = 0;
};


#endif // QN_TOOL_TIP_QUERYABLE_H
