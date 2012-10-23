#ifndef QN_CONTEXT_HELP_QUERYABLE_H
#define QN_CONTEXT_HELP_QUERYABLE_H

#include <QtCore/QPointF>

class QObject;

class QnContextHelpQueryable {
public:
    virtual ~QnContextHelpQueryable() {}

    virtual int helpTopicAt(const QPointF &pos) const = 0;
};



#define HelpTopicId _id("_qn_contextHelpId")

void setHelpTopicId(QObject *object, int helpTopic);

inline void setHelpTopicId(QObject *object0, QObject *object1, int helpTopic) {
    setHelpTopicId(object0, helpTopic);
    setHelpTopicId(object1, helpTopic);
}

#endif // QN_CONTEXT_HELP_QUERYABLE_H
