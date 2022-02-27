// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "help_topic_accessor.h"

#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QWidget>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QAbstractItemView>

#include <utils/common/variant.h>

#include <ui/common/help_topic_queryable.h>
#include <client/client_globals.h>
#include <common/common_globals.h>

#include "help_topics.h"

namespace {
    const char *qn_helpTopicPropertyName = "_qn_contextHelpId";

    int topic(int id) {
        if (id == Qn::Empty_Help || id == Qn::Forced_Empty_Help)
            return Qn::Empty_Help;
        return qAbs(id);
    }
}

int QnHelpTopicAccessor::helpTopic(const QWidget* object)
{
    if (!NX_ASSERT(object))
        return Qn::Empty_Help;

    // Iterating through all the hierarchy to get a proper help topic
    for (auto widget = object; widget; widget = widget->parentWidget())
    {
        const auto property = widget->property(qn_helpTopicPropertyName);
        const auto topicId = qAbs(qvariant_cast<int>(property, Qn::Empty_Help));
        if (topicId == Qn::Forced_Empty_Help)
            break;
        if (topicId != Qn::Empty_Help)
            return topicId;
    }

    return Qn::Empty_Help;
}

int QnHelpTopicAccessor::helpTopic(QObject *object)
{
    if (!NX_ASSERT(object))
        return Qn::Empty_Help;

    return qAbs(qvariant_cast<int>(object->property(qn_helpTopicPropertyName), Qn::Empty_Help));
}

void QnHelpTopicAccessor::setHelpTopic(QObject *object, int helpTopic, bool enforceForChildren) {
    if (!NX_ASSERT(object))
        return;

    if (helpTopic == Qn::Forced_Empty_Help)
        enforceForChildren = false;

    object->setProperty(qn_helpTopicPropertyName, enforceForChildren ? -helpTopic : helpTopic);
}

int QnHelpTopicAccessor::helpTopicAt(QWidget *widget, const QPoint &pos, bool bubbleUp) {
    QPoint widgetPos = pos;

    while(true) {
        int topicId = qvariant_cast<int>(widget->property(qn_helpTopicPropertyName), Qn::Empty_Help);

        if(topicId != Qn::Empty_Help)
            return topic(topicId);

        if(HelpTopicQueryable *queryable = dynamic_cast<HelpTopicQueryable *>(widget)) {
            topicId = queryable->helpTopicAt(widgetPos);
            if(topicId != Qn::Empty_Help)
                return topic(topicId);
        }

        if(QGraphicsView *view = dynamic_cast<QGraphicsView *>(widget)) {
            if(view->scene()) {
                topicId = qvariant_cast<int>(view->scene()->property(qn_helpTopicPropertyName), Qn::Empty_Help);
                if(topicId < 0)
                    return -topicId;
                if(topicId == Qn::Forced_Empty_Help)
                    return Qn::Empty_Help;
            }

            QPointF scenePos = view->mapToScene(widgetPos);
            foreach(QGraphicsItem *item, view->items(widgetPos)) {
                int topicId = helpTopicAt(item, item->mapFromScene(scenePos));
                if(topicId != Qn::Empty_Help)
                    return topic(topicId);
            }

            if(topicId >= 0)
                return topic(topicId);
        }

        if(QAbstractItemView *view = dynamic_cast<QAbstractItemView *>(widget)) {
            if(QAbstractItemModel *model = view->model()) {
                QPoint viewportPos = view->viewport()->mapFrom(view, widgetPos);
                topicId = qvariant_cast<int>(model->data(view->indexAt(viewportPos), Qn::HelpTopicIdRole), Qn::Empty_Help);
                if(topicId != Qn::Empty_Help)
                    return topic(topicId);
            }
        }

        if(!bubbleUp)
            break;

        if(!widget->isWindow() && widget->parentWidget()) {
            widgetPos = widget->mapToParent(widgetPos);
            widget = widget->parentWidget();
        } else {
            break;
        }
    }

    return Qn::Empty_Help;
}

int QnHelpTopicAccessor::helpTopicAt(QGraphicsItem *item, const QPointF &pos, bool bubbleUp) {
    QPointF itemPos = pos;

    while(true) {
        int topicId = Qn::Empty_Help;

        if(QGraphicsObject *object = item->toGraphicsObject()) {
            topicId = qvariant_cast<int>(object->property(qn_helpTopicPropertyName), Qn::Empty_Help);
            if(topicId != Qn::Empty_Help)
                return topic(topicId);
        }

        if(HelpTopicQueryable *queryable = dynamic_cast<HelpTopicQueryable *>(item)) {
            topicId = queryable->helpTopicAt(itemPos);
            if(topicId != Qn::Empty_Help)
                return topic(topicId);
        }

        if(QGraphicsProxyWidget *proxy = dynamic_cast<QGraphicsProxyWidget *>(item)) {
            if(proxy->widget()) {
                QPoint widgetPos = itemPos.toPoint();
                QWidget *child = proxy->widget()->childAt(widgetPos);
                if(!child)
                    child = proxy->widget();

                topicId = helpTopicAt(child, child->mapFrom(proxy->widget(), widgetPos), true);
                if(topicId != Qn::Empty_Help)
                    return topic(topicId);
            }
        }

        if(!bubbleUp)
            break;

        if(item->parentItem()) {
            itemPos = item->mapToParent(itemPos);
            item = item->parentItem();
        } else {
            break;
        }
    }

    return Qn::Empty_Help;
}

int QnHelpTopicAccessor::helpTopicAt(QQuickItem* item, const QPointF& pos)
{
    if (!NX_ASSERT(item) || !item->isVisible() || !item->contains(pos))
        return Qn::Empty_Help;

    const auto children = item->childItems();
    for (const auto child: children)
    {
        const int topic = QnHelpTopicAccessor::helpTopicAt(child, child->mapFromItem(item, pos));
        if (topic != Qn::Empty_Help)
            return topic;
    }

    return QnHelpTopicAccessor::helpTopic(item);
}
