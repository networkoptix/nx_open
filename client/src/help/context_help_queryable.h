#ifndef QN_CONTEXT_HELP_QUERYABLE_H
#define QN_CONTEXT_HELP_QUERYABLE_H

#include <QtCore/QPointF>

class QWidget;

class QnContextHelpQueryable {
public:
    virtual ~QnContextHelpQueryable() {}

    virtual int helpTopicAt(const QPointF &pos) const = 0;
};



#define HelpTopicId _id("_qn_contextHelpId")

void setHelpTopicId(QWidget *widget, int helpTopic);

inline void setHelpTopicId(QWidget *widget0, QWidget *widget1, int helpTopic) {
    setHelpTopicId(widget0, helpTopic);
    setHelpTopicId(widget1, helpTopic);
}

#endif // QN_CONTEXT_HELP_QUERYABLE_H
