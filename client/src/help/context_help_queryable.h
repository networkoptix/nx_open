#ifndef QN_CONTEXT_HELP_QUERYABLE_H
#define QN_CONTEXT_HELP_QUERYABLE_H

#include <QtCore/QPointF>

#include "help_topics.h"

class QObject;

class QnContextHelpQueryable {
public:
    virtual ~QnContextHelpQueryable() {}

    virtual Qn::HelpTopic helpTopicAt(const QPointF &pos) const = 0;
};



#define HelpTopicId _id("_qn_contextHelpId")

void setHelpTopicId(QObject *object, Qn::HelpTopic helpTopic);

inline void setHelpTopicId(QObject *object0, QObject *object1, Qn::HelpTopic helpTopic) {
    setHelpTopicId(object0, helpTopic);
    setHelpTopicId(object1, helpTopic);
}

#endif // QN_CONTEXT_HELP_QUERYABLE_H
