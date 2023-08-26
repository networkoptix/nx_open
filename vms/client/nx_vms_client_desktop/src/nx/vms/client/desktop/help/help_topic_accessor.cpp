// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "help_topic_accessor.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QObject>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QWidget>

#include <client/client_globals.h>
#include <common/common_globals.h>
#include <ui/common/help_topic_queryable.h>
#include <utils/common/variant.h>

#include "help_topic.h"

namespace nx::vms::client::desktop {

namespace {
const char* qn_helpTopicPropertyName = "_qn_contextHelpId";

int topic(int id)
{
    if (id == HelpTopic::Id::Empty || id == HelpTopic::Id::Forced_Empty)
        return HelpTopic::Id::Empty;
    return qAbs(id);
}
} // namespace

int HelpTopicAccessor::helpTopic(const QWidget* object)
{
    if (!NX_ASSERT(object))
        return HelpTopic::Id::Empty;

    // Iterating through all the hierarchy to get a proper help topic
    for (auto widget = object; widget; widget = widget->parentWidget())
    {
        const auto property = widget->property(qn_helpTopicPropertyName);
        const auto topicId = qAbs(qvariant_cast<int>(property, HelpTopic::Id::Empty));
        if (topicId == HelpTopic::Id::Forced_Empty)
            break;
        if (topicId != HelpTopic::Id::Empty)
            return topicId;
    }

    return HelpTopic::Id::Empty;
}

int HelpTopicAccessor::helpTopic(QObject* object)
{
    if (!NX_ASSERT(object))
        return HelpTopic::Id::Empty;

    return qAbs(
        qvariant_cast<int>(object->property(qn_helpTopicPropertyName), HelpTopic::Id::Empty));
}

void HelpTopicAccessor::setHelpTopic(QObject* object, int helpTopic, bool enforceForChildren)
{
    if (!NX_ASSERT(object))
        return;

    if (helpTopic == HelpTopic::Id::Forced_Empty)
        enforceForChildren = false;

    object->setProperty(qn_helpTopicPropertyName, enforceForChildren ? -helpTopic : helpTopic);
}

int HelpTopicAccessor::helpTopicAt(QWidget* widget, const QPoint& pos, bool bubbleUp)
{
    QPoint widgetPos = pos;

    while (true)
    {
        int topicId =
            qvariant_cast<int>(widget->property(qn_helpTopicPropertyName), HelpTopic::Id::Empty);

        if (topicId != HelpTopic::Id::Empty)
            return topic(topicId);

        if (HelpTopicQueryable* queryable = dynamic_cast<HelpTopicQueryable*>(widget))
        {
            topicId = queryable->helpTopicAt(widgetPos);
            if (topicId != HelpTopic::Id::Empty)
                return topic(topicId);
        }

        if (QGraphicsView* view = dynamic_cast<QGraphicsView*>(widget))
        {
            if (view->scene())
            {
                topicId = qvariant_cast<int>(
                    view->scene()->property(qn_helpTopicPropertyName), HelpTopic::Id::Empty);
                if (topicId < 0)
                    return -topicId;
                if (topicId == HelpTopic::Id::Forced_Empty)
                    return HelpTopic::Id::Empty;
            }

            QPointF scenePos = view->mapToScene(widgetPos);
            foreach (QGraphicsItem* item, view->items(widgetPos))
            {
                int topicId = helpTopicAt(item, item->mapFromScene(scenePos));
                if (topicId != HelpTopic::Id::Empty)
                    return topic(topicId);
            }

            if (topicId >= 0)
                return topic(topicId);
        }

        if (QAbstractItemView* view = dynamic_cast<QAbstractItemView*>(widget))
        {
            if (QAbstractItemModel* model = view->model())
            {
                QPoint viewportPos = view->viewport()->mapFrom(view, widgetPos);
                topicId = qvariant_cast<int>(
                    model->data(view->indexAt(viewportPos), Qn::HelpTopicIdRole),
                    HelpTopic::Id::Empty);
                if (topicId != HelpTopic::Id::Empty)
                    return topic(topicId);
            }
        }

        if (!bubbleUp)
            break;

        if (!widget->isWindow() && widget->parentWidget())
        {
            widgetPos = widget->mapToParent(widgetPos);
            widget = widget->parentWidget();
        }
        else
        {
            break;
        }
    }

    return HelpTopic::Id::Empty;
}

int HelpTopicAccessor::helpTopicAt(QGraphicsItem* item, const QPointF& pos, bool bubbleUp)
{
    QPointF itemPos = pos;

    while (true)
    {
        int topicId = HelpTopic::Id::Empty;

        if (const auto object = item->toGraphicsObject())
        {
            topicId = qvariant_cast<int>(
                object->property(qn_helpTopicPropertyName), HelpTopic::Id::Empty);
            if (topicId != HelpTopic::Id::Empty)
                return topic(topicId);
        }

        if (const auto queryable = dynamic_cast<HelpTopicQueryable*>(item))
        {
            topicId = queryable->helpTopicAt(itemPos);
            if (topicId != HelpTopic::Id::Empty)
                return topic(topicId);
        }

        if (const auto proxy = dynamic_cast<QGraphicsProxyWidget*>(item))
        {
            if (proxy->widget())
            {
                QPoint widgetPos = itemPos.toPoint();
                QWidget* child = proxy->widget()->childAt(widgetPos);
                if (!child)
                    child = proxy->widget();

                topicId = helpTopicAt(child, child->mapFrom(proxy->widget(), widgetPos), true);
                if (topicId != HelpTopic::Id::Empty)
                    return topic(topicId);
            }
        }

        if (!bubbleUp)
            break;

        if (item->parentItem())
        {
            itemPos = item->mapToParent(itemPos);
            item = item->parentItem();
        }
        else
        {
            break;
        }
    }

    return HelpTopic::Id::Empty;
}

int HelpTopicAccessor::helpTopicAt(QQuickItem* item, const QPointF& pos)
{
    if (!NX_ASSERT(item) || !item->isVisible() || !item->contains(pos))
        return HelpTopic::Id::Empty;

    const auto children = item->childItems();
    for (const auto child: children)
    {
        const int topic = helpTopicAt(child, child->mapFromItem(item, pos));
        if (topic != HelpTopic::Id::Empty)
            return topic;
    }

    return helpTopic(item);
}

} // namespace nx::vms::client::desktop
