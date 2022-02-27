// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_HELP_TOPIC_ACCESSOR_H
#define QN_HELP_TOPIC_ACCESSOR_H

#include <QtCore/QPoint>
#include <QtCore/QPointF>

class QObject;
class QWidget;
class QGraphicsItem;
class QQuickItem;

class QnHelpTopicAccessor {
public:
    static void setHelpTopic(QObject *object, int helpTopic, bool enforceForChildren = false);

    static void setHelpTopic(QObject *object0, QObject *object1, int helpTopic)
    {
        setHelpTopic(object0, helpTopic);
        setHelpTopic(object1, helpTopic);
    }

    static int helpTopicAt(QWidget *widget, const QPoint &pos, bool bubbleUp = false);
    static int helpTopicAt(QGraphicsItem *item, const QPointF &pos, bool bubbleUp = false);
    static int helpTopicAt(QQuickItem* item, const QPointF& pos); //< Always from parent down.
    static int helpTopic(QObject* object);
    static int helpTopic(const QWidget* object);
};

inline int helpTopic(QObject *object) { return QnHelpTopicAccessor::helpTopic(object); }
inline int helpTopic(const QWidget *object) { return QnHelpTopicAccessor::helpTopic(object); }
inline void setHelpTopic(QObject *object, int helpTopic, bool enforceForChildren = false) { QnHelpTopicAccessor::setHelpTopic(object, helpTopic, enforceForChildren); }
inline void setHelpTopic(QObject *object0, QObject *object1, int helpTopic) { QnHelpTopicAccessor::setHelpTopic(object0, object1, helpTopic); }

#endif // QN_HELP_TOPIC_ACCESSOR_H
