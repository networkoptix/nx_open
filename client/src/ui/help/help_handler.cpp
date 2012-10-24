#include "help_handler.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QDesktopServices>
#include <QtGui/QCursor>
#include <QtGui/QHelpEvent>
#include <QtGui/QWhatsThis>

#include "help_topic_accessor.h"

namespace {
    const char *relativeUrlForTopic(int topic) {
        switch(topic) {
#define QN_HELP_TOPIC(ID, URL)                                                  \
        case Qn::ID: return URL;
#include "help_topics.i"
        default:
            return NULL;
        }
    }

} // anonymous namespace


QnHelpHandler::QnHelpHandler(QObject *parent):
    QObject(parent),
    m_topic(Qn::Empty_Help)
{
    m_helpRoot = qApp->applicationDirPath() + QLatin1String("/help");
}

QnHelpHandler::~QnHelpHandler() {
    return;
}

void QnHelpHandler::show() {
    return;
}

void QnHelpHandler::hide() {
    return;
}

void QnHelpHandler::setHelpTopic(int topic) {
    m_topic = topic;

    QDesktopServices::openUrl(urlForTopic(topic));
}

QUrl QnHelpHandler::urlForTopic(int topic) const {
    return QUrl::fromLocalFile(m_helpRoot + QLatin1String("/") + QLatin1String(relativeUrlForTopic(topic)));
}

bool QnHelpHandler::eventFilter(QObject *watched, QEvent *event) {
    QWidget *widget = qobject_cast<QWidget *>(watched);
    if(!widget)
        return false;

    switch(event->type()) {
    case QEvent::QueryWhatsThis: {
        bool accepted = QnHelpTopicAccessor::helpTopicAt(widget, widget->mapFromGlobal(QCursor::pos())) != -1;
        event->setAccepted(accepted);
        return accepted;
    }
    case QEvent::WhatsThis: {
        int topicId = QnHelpTopicAccessor::helpTopicAt(widget, static_cast<QHelpEvent *>(event)->pos());

        if(topicId != -1) {
            event->accept();

            setHelpTopic(topicId);
            show();

            QWhatsThis::leaveWhatsThisMode();
            return true;
        }

        return false;
    }
    default:
        return false;
    }
}



