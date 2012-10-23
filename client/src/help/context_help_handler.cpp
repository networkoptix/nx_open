#include "context_help_handler.h"

#include <QtCore/QEvent>
#include <QtGui/QCursor>
#include <QtGui/QHelpEvent>
#include <QtGui/QWhatsThis>
#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsObject>

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

QnContextHelpQueryable *QnContextHelpHandler::queryable(QObject *object) const {
    return dynamic_cast<QnContextHelpQueryable *>(object);
}

QnContextHelpQueryable *QnContextHelpHandler::queryable(QGraphicsItem *item) const {
    return dynamic_cast<QnContextHelpQueryable *>(item);
}

QGraphicsView *QnContextHelpHandler::view(QObject *object) const {
    return dynamic_cast<QGraphicsView *>(object);
}

int QnContextHelpHandler::helpTopicAt(QGraphicsItem *item, const QPointF &pos) const {
    if(QGraphicsObject *object = item->toGraphicsObject()) {
        int topicId = qvariant_cast<int>(object->property(Qn::HelpTopicId), -1);
        if(topicId != -1)
            return topicId;
    }

    if(QnContextHelpQueryable *queryable = this->queryable(item))
        return queryable->helpTopicAt(pos);
}

int QnContextHelpHandler::helpTopicAt(QWidget *widget, const QPoint &pos) const {
    int topicId = qvariant_cast<int>(widget->property(Qn::HelpTopicId), -1);
    if(topicId != -1)
        return topicId;

    if(QnContextHelpQueryable *queryable = this->queryable(widget))
        return queryable->helpTopicAt(pos);

    if(QGraphicsView *view = this->view(widget)) {
        QPointF scenePos = view->mapToScene(pos);
        foreach(QGraphicsItem *item, view->items(pos)) {
            int topicId = helpTopicAt(item, item->mapFromScene(scenePos));
            if(topicId != -1)
                return topicId;
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


