#include "context_help_handler.h"

#include <QtCore/QEvent>
#include <QtGui/QHelpEvent>
#include <QtGui/QWhatsThis>

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

QnContextHelpQueryable *QnContextHelpHandler::queryable(QObject *object) {
    return dynamic_cast<QnContextHelpQueryable *>(object);
}

bool QnContextHelpHandler::eventFilter(QObject *watched, QEvent *event) {
    switch(event->type()) {
    case QEvent::QueryWhatsThis:
        if(qvariant_cast<int>(watched->property(Qn::HelpTopicId), -1) != -1 || queryable(watched)) {
            event->accept();
            return true;
        }
       
        return false;
    case QEvent::WhatsThis: {
        int topicId = -1;
        if(QnContextHelpQueryable *queryable = this->queryable(watched)) {
            topicId = queryable->helpTopicAt(static_cast<QHelpEvent *>(event)->pos());
        } else {
            topicId = qvariant_cast<int>(watched->property(Qn::HelpTopicId), -1);
        }

        if(topicId != -1) {
            event->accept();

            if(contextHelp()) {
                contextHelp()->setHelpTopic(static_cast<Qn::HelpTopic>(topicId));
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


