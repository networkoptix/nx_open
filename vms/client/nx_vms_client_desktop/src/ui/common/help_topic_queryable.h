#ifndef QN_HELP_TOPIC_QUERYABLE_H
#define QN_HELP_TOPIC_QUERYABLE_H

#include <QtCore/QPointF>

class HelpTopicQueryable {
public:
    virtual ~HelpTopicQueryable() {}

    virtual int helpTopicAt(const QPointF &pos) const = 0;
};

#endif // QN_HELP_TOPIC_QUERYABLE_H
