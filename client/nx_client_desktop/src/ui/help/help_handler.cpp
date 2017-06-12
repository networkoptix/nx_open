#include "help_handler.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtGui/QDesktopServices>
#include <QtGui/QCursor>
#include <QtGui/QHelpEvent>
#include <QtWidgets/QWhatsThis>
#include <QtWidgets/QWidget>

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

#ifdef Q_OS_MAC
    const QString relativeHelpRootPath = lit("/../Resources/help/");
#else
    const QString relativeHelpRootPath = lit("/help/");
#endif

} // anonymous namespace


QnHelpHandler::QnHelpHandler(QObject* parent):
    QObject(parent),
    m_topic(Qn::Empty_Help)
{
    m_helpSearchPaths.append(qApp->applicationDirPath() + relativeHelpRootPath);
    m_helpSearchPaths.append(qApp->applicationDirPath() + lit("/..") + relativeHelpRootPath);
}

QnHelpHandler::~QnHelpHandler()
{
}

void QnHelpHandler::setHelpTopic(int topic) {
    m_topic = topic;

    QDesktopServices::openUrl(urlForTopic(topic));
}

QUrl QnHelpHandler::urlForTopic(int topic) const
{
    QString topicPath = QLatin1String(relativeUrlForTopic(topic));

    for (const QString& helpRoot: m_helpSearchPaths)
    {
        QString filePath = helpRoot + topicPath;
        if (QFile::exists(filePath))
            return QUrl::fromLocalFile(filePath);
    }

    return QUrl();
}

bool QnHelpHandler::eventFilter(QObject *watched, QEvent *event) {
    QWidget *widget = qobject_cast<QWidget *>(watched);
    if(!widget)
        return false;

    switch(event->type()) {
    case QEvent::QueryWhatsThis: {
        bool accepted = QnHelpTopicAccessor::helpTopicAt(widget, widget->mapFromGlobal(QCursor::pos())) != Qn::Empty_Help;
        event->setAccepted(accepted);
        return accepted;
    }
    case QEvent::WhatsThis: {
        QHelpEvent *e = static_cast<QHelpEvent *>(event);

        int topicId = QnHelpTopicAccessor::helpTopicAt(widget, e->pos(), true);

        if(topicId != Qn::Empty_Help) {
            event->accept();

            setHelpTopic(topicId);

            QWhatsThis::leaveWhatsThisMode();
            return true;
        }

        return false;
    }
    case QEvent::MouseButtonPress: {
        QMouseEvent *e = static_cast<QMouseEvent *>(event);

        if(e->button() == Qt::RightButton && QWhatsThis::inWhatsThisMode()) {
            QWhatsThis::leaveWhatsThisMode();
            return true;
        }

        return false;
    }
    default:
        return false;
    }
}



