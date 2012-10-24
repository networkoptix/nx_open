#ifndef QN_HELP_TOPIC_ACCESSOR_H
#define QN_HELP_TOPIC_ACCESSOR_H

#include <QtCore/QPoint>
#include <QtCore/QPointF>

class QObject;
class QWidget;
class QGraphicsItem;

class QnHelpTopicAccessor {
public:
    static void setHelpTopic(QObject *object, int helpTopic);

    static inline void setHelpTopic(QObject *object0, QObject *object1, int helpTopic) {
        setHelpTopic(object0, helpTopic);
        setHelpTopic(object1, helpTopic);
    }

    static int helpTopicAt(QWidget *widget, const QPoint &pos, bool bubbleUp = false);
    static int helpTopicAt(QGraphicsItem *item, const QPointF &pos, bool bubbleUp = false);
};


inline void setHelpTopic(QObject *object, int helpTopic) { QnHelpTopicAccessor::setHelpTopic(object, helpTopic); }
inline void setHelpTopic(QObject *object0, QObject *object1, int helpTopic) { QnHelpTopicAccessor::setHelpTopic(object0, object1, helpTopic); }

#endif // QN_HELP_TOPIC_ACCESSOR_H
