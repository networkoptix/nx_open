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

#ifdef Q_OS_MAC
    const QString relativeHelpRootPath = lit("/help");
#else
    const QString relativeHelpRootPath = lit("/../help");
#endif

    /** If the help could not be loaded, try again in 30 seconds. */
    const int defaultHelpRetryPeriodMSec = 30*1000;

    /** Send requests at least once a hour. */
    const int maximumHelpRetryPeriodMSec = 60*60*1000;

} // anonymous namespace


QnHelpHandler::QnHelpHandler(QObject *parent):
    QObject(parent),
    m_topic(Qn::Empty_Help),
    m_helpRetryPeriodMSec(defaultHelpRetryPeriodMSec)
{
    m_helpRoot = qApp->applicationDirPath() + relativeHelpRootPath;

    QnOnlineHelpDetector *helpDetector = new QnOnlineHelpDetector(this);
    connect(helpDetector,   &QnOnlineHelpDetector::urlFetched,  this,   [this, helpDetector](const QString &helpUrl) {
        m_onlineHelpRoot = helpUrl;
        helpDetector->deleteLater();
    });

    connect(helpDetector,   &QnOnlineHelpDetector::error,       this,  [this, helpDetector] {
         QTimer::singleShot(m_helpRetryPeriodMSec, helpDetector, SLOT(fetchHelpUrl()));

         /* Try again in the double time but at least once a maximumHelpRetryPeriodMSec. */
         if (m_helpRetryPeriodMSec < maximumHelpRetryPeriodMSec)
            m_helpRetryPeriodMSec = qMin(m_helpRetryPeriodMSec * 2, maximumHelpRetryPeriodMSec);
    });
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
    else
        QMessageBox::warning(0, tr("Error"), tr("Error")); // TODO: #dklychkov place something more detailed in the future

    return QUrl();
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



