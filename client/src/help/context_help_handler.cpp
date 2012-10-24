#include "context_help_handler.h"

#include <QtCore/QEvent>
#include <QtGui/QCursor>
#include <QtGui/QHelpEvent>
#include <QtGui/QWhatsThis>
#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsObject>
#include <QtGui/QGraphicsProxyWidget>

#include <utils/common/variant.h>

#include "context_help.h"
#include "context_help_queryable.h"

QnContextHelpHandler::QnContextHelpHandler(QObject *parent):
    QObject(parent)
{}

QnContextHelpHandler::~QnContextHelpHandler() {
    return;
}

QnContextHelp *QnContextHelpHandler::contextHelp() const {
    return m_contextHelp.data();
}

void QnContextHelpHandler::setContextHelp(QnContextHelp *contextHelp) {
    m_contextHelp = contextHelp;
}

int QnContextHelpHandler::helpTopicAt(QGraphicsItem *item, const QPointF &pos) const {
    if(QGraphicsObject *object = item->toGraphicsObject()) {
        int topicId = qvariant_cast<int>(object->property(Qn::HelpTopicId), -1);
        if(topicId != -1)
            return topicId;
    }

    if(HelpTopicQueryable *queryable = dynamic_cast<HelpTopicQueryable *>(item))
        return queryable->helpTopicAt(pos);

    if(QGraphicsProxyWidget *proxy = dynamic_cast<QGraphicsProxyWidget *>(item)) {
        if(proxy->widget()) {
            QPoint widgetPos = pos.toPoint();
            QWidget *child = proxy->widget()->childAt(widgetPos);
            if(!child)
                child = proxy->widget();

            return helpTopicAt(child, widgetPos, true);
        }
    }

    return -1;
}

int QnContextHelpHandler::helpTopicAt(QWidget *widget, const QPoint &pos, bool bubbleUp) const {
    QPoint widgetPos = pos;

    while(true) {
        int topicId = qvariant_cast<int>(widget->property(Qn::HelpTopicId), -1);
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
                topicId = qvariant_cast<int>(view->scene()->property(Qn::HelpTopicId), -1);
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

bool QnContextHelpHandler::eventFilter(QObject *watched, QEvent *event) {
    QWidget *widget = qobject_cast<QWidget *>(watched);
    if(!widget)
        return false;

    switch(event->type()) {
    case QEvent::QueryWhatsThis: {
        bool accepted = helpTopicAt(widget, widget->mapFromGlobal(QCursor::pos())) != -1;
        event->setAccepted(accepted);
        return accepted;
    }
    case QEvent::WhatsThis: {
        int topicId = helpTopicAt(widget, static_cast<QHelpEvent *>(event)->pos());
        
        if(topicId != -1) {
            event->accept();

            if(contextHelp()) {
                contextHelp()->setHelpTopic(topicId);
                contextHelp()->show();
            }

            QWhatsThis::leaveWhatsThisMode();
            return true;
        }

        return false;
    }
    default:
        return false;
    }
}


