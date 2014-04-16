#include "help_handler.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtGui/QDesktopServices>
#include <QtGui/QCursor>
#include <QtGui/QHelpEvent>
#include <QtWidgets/QWhatsThis>
#include <QtWidgets/QWidget>
#include <QtWidgets/QMessageBox>

#include "help_topic_accessor.h"
#include "online_help_detector.h"

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
    m_helpRoot = qApp->applicationDirPath() + QLatin1String("/../help");

    QnOnlineHelpDetector *helpUrlDetector = new QnOnlineHelpDetector(this);
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
    QString filePath = m_helpRoot + QLatin1String("/") + QLatin1String(relativeUrlForTopic(topic));
    if (QFile::exists(filePath))
        return QUrl::fromLocalFile(filePath);
    else if (!m_onlineHelpRoot.isEmpty())
        return QUrl(m_onlineHelpRoot + lit("/") + QLatin1String(relativeUrlForTopic(topic)));
    else {
        QMessageBox::warning(0, tr("Error"), tr("Error")); // TODO: #dklychkov place something more detailed in the future
    }
}

void QnHelpHandler::at_helpUrlDetector_urlFetched(const QString &helpUrl) {
    QnOnlineHelpDetector *detector = qobject_cast<QnOnlineHelpDetector*>(sender());
    if (!detector)
        return;

    detector->deleteLater();
    m_onlineHelpRoot = helpUrl;
}

void QnHelpHandler::at_helpUrlDetector_error() {
    QnOnlineHelpDetector *detector = qobject_cast<QnOnlineHelpDetector*>(sender());
    if (!detector)
        return;

    QTimer::singleShot(30 * 1000, detector, SLOT(fetchHelpUrl()));
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
        QHelpEvent *e = static_cast<QHelpEvent *>(event);

        int topicId = QnHelpTopicAccessor::helpTopicAt(widget, e->pos(), true);

        if(topicId != -1) {
            event->accept();

            setHelpTopic(topicId);
            show();

            QWhatsThis::leaveWhatsThisMode();
            return true;
        }

        return false;
    }
    case QEvent::MouseButtonPress: {
        QMouseEvent *e = static_cast<QMouseEvent *>(event);
        
        if(e->button() == Qt::RightButton)
            QWhatsThis::leaveWhatsThisMode();

        return false;
    }
    default:
        return false;
    }
}



