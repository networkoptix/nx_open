#ifndef QN_CONTEXT_HELP_QUERYABLE_H
#define QN_CONTEXT_HELP_QUERYABLE_H

#include <QtCore/QPointF>

class QnContextHelpQueryable {
public:
    virtual ~QnContextHelpQueryable() {}

    virtual int helpTopicAt(const QPointF &pos) const = 0;
};

#define HelpTopicId _id("_qn_contextHelpId")

#endif // QN_CONTEXT_HELP_QUERYABLE_H
