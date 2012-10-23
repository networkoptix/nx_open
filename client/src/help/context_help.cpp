#include "context_help.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QDesktopServices>

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


QnContextHelp::QnContextHelp(QObject *parent):
    QObject(parent),
    m_topic(Qn::Empty_Help)
{
    m_helpRoot = qApp->applicationDirPath() + QLatin1String("/help");
}

QnContextHelp::~QnContextHelp() {
    return;
}

void QnContextHelp::show() {
    return;
}

void QnContextHelp::hide() {
    return;
}

void QnContextHelp::setHelpTopic(int topic) {
    m_topic = topic;

    QDesktopServices::openUrl(urlForTopic(topic));
}

QUrl QnContextHelp::urlForTopic(int topic) const {
    return QUrl::fromLocalFile(m_helpRoot + QLatin1String("/") + QLatin1String(relativeUrlForTopic(topic)));
}


