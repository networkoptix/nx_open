// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPoint>

class QObject;
class QWidget;
class QGraphicsItem;
class QQuickItem;

namespace nx::vms::client::desktop {

class HelpTopicAccessor
{
public:
    static void setHelpTopic(QObject* object, int helpTopic, bool enforceForChildren = false);

    static void setHelpTopic(QObject* object0, QObject* object1, int helpTopic)
    {
        setHelpTopic(object0, helpTopic);
        setHelpTopic(object1, helpTopic);
    }

    static int helpTopicAt(QWidget* widget, const QPoint& pos, bool bubbleUp = false);
    static int helpTopicAt(QGraphicsItem* item, const QPointF& pos, bool bubbleUp = false);
    static int helpTopicAt(QQuickItem* item, const QPointF& pos); //< Always from parent down.
    static int helpTopic(QObject* object);
    static int helpTopic(const QWidget* object);
};

inline int helpTopic(QObject* object)
{
    return HelpTopicAccessor::helpTopic(object);
}

inline int helpTopic(const QWidget* object)
{
    return HelpTopicAccessor::helpTopic(object);
}

inline void setHelpTopic(QObject* object, int helpTopic, bool enforceForChildren = false)
{
    HelpTopicAccessor::setHelpTopic(object, helpTopic, enforceForChildren);
}

inline void setHelpTopic(QObject* object0, QObject* object1, int helpTopic)
{
    HelpTopicAccessor::setHelpTopic(object0, object1, helpTopic);
}

} // namespace nx::vms::client::desktop
