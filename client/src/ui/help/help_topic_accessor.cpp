#include "help_topic_accessor.h"

#include <QtCore/QObject>
#include <QtGui/QWidget>
#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsProxyWidget>

#include <utils/common/warnings.h>
#include <utils/common/variant.h>

#include "help_topic_queryable.h"

namespace {
    const char *qn_helpTopicPropertyName = "_qn_contextHelpId";
}

void QnHelpTopicAccessor::setHelpTopic(QObject *object, int helpTopic) {
    if(!object) {
        qnNullWarning(object);
        return;
    }

    object->setProperty(qn_helpTopicPropertyName, helpTopic);
}

int QnHelpTopicAccessor::helpTopicAt(QWidget *widget, const QPoint &pos, bool bubbleUp) {
    QPoint widgetPos = pos;

    while(true) {
        int topicId = qvariant_cast<int>(widget->property(qn_helpTopicPropertyName), -1);
        if(topicId != -1)
            return topicId;

        if(HelpTopicQueryable *queryable = dynamic_cast<HelpTopicQueryable *>(widget))
            return queryable->helpTopicAt(widgetPos);

        if(QGraphicsView *view = dynamic_cast<QGraphicsView *>(widget)) {
            QPointF scenePos = view->mapToScene(widgetPos);
            foreach(QGraphicsItem *item, view->items(widgetPos)) {
                int topicId = helpTopicAt(item, item->mapFromScene(scenePos));
                if(topicId != -1)
                    return topicId;
            }

            if(view->scene()) {
                topicId = qvariant_cast<int>(view->scene()->property(qn_helpTopicPropertyName), -1);
                if(topicId != -1)
                    return topicId;
            }
        }

        if(!bubbleUp)
            break;

        if(widget->parentWidget()) {
            widgetPos = widget->mapToParent(widgetPos);
            widget = widget->parentWidget();
        } else {
            break;
        }
    }

    return -1;
}

int QnHelpTopicAccessor::helpTopicAt(QGraphicsItem *item, const QPointF &pos, bool bubbleUp) {
    QPointF itemPos = pos;

    while(true) {
        if(QGraphicsObject *object = item->toGraphicsObject()) {
            int topicId = qvariant_cast<int>(object->property(qn_helpTopicPropertyName), -1);
            if(topicId != -1)
                return topicId;
        }

        if(HelpTopicQueryable *queryable = dynamic_cast<HelpTopicQueryable *>(item))
            return queryable->helpTopicAt(itemPos);

        if(QGraphicsProxyWidget *proxy = dynamic_cast<QGraphicsProxyWidget *>(item)) {
            if(proxy->widget()) {
                QPoint widgetPos = itemPos.toPoint();
                QWidget *child = proxy->widget()->childAt(widgetPos);
                if(!child)
                    child = proxy->widget();

                return helpTopicAt(child, widgetPos, true);
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

    return -1;
}
