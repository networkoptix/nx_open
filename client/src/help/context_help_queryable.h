#ifndef QN_CONTEXT_HELP_QUERYABLE_H
#define QN_CONTEXT_HELP_QUERYABLE_H

#include <QtCore/QPointF>

#include "help_topics.h"

class QWidget;

class QnContextHelpQueryable {
public:
    virtual ~QnContextHelpQueryable() {}

    virtual Qn::HelpTopic helpTopicAt(const QPointF &pos) const = 0;
};



#define HelpTopicId _id("_qn_contextHelpId")

void setHelpTopicId(QWidget *widget, Qn::HelpTopic helpTopic);

inline void setHelpTopicId(QWidget *widget0, QWidget *widget1, Qn::HelpTopic helpTopic) {
    setHelpTopicId(widget0, helpTopic);
    setHelpTopicId(widget1, helpTopic);
}

#endif // QN_CONTEXT_HELP_QUERYABLE_H
