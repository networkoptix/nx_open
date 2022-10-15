// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "help_handler.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtGui/QDesktopServices>
#include <QtGui/QCursor>
#include <QtGui/QHelpEvent>
#include <QtWidgets/QWhatsThis>
#include <QtWidgets/QWidget>

#include "help_topic_accessor.h"

#include <nx/vms/utils/external_resources.h>

QnHelpHandler::QnHelpHandler(QObject* parent):
    QObject(parent),
    m_topic(Qn::HelpTopic::Empty_Help)
{
}

QnHelpHandler::~QnHelpHandler()
{
}

void QnHelpHandler::setHelpTopic(int topic)
{
    setHelpTopic(static_cast<Qn::HelpTopic>(topic));
}

void QnHelpHandler::setHelpTopic(Qn::HelpTopic topic)
{
    m_topic = topic;
    openHelpTopic(topic);
}

void QnHelpHandler::openHelpTopic(int topic)
{
    openHelpTopic(static_cast<Qn::HelpTopic>(topic));
}

void QnHelpHandler::openHelpTopic(Qn::HelpTopic topic)
{
    QDesktopServices::openUrl(urlForTopic(static_cast<Qn::HelpTopic>(topic)));
}

QUrl QnHelpHandler::urlForTopic(Qn::HelpTopic topic)
{
    const auto helpRoot = nx::vms::utils::externalResourcesDirectory().absolutePath() + "/help/";
    QString filePath = helpRoot + relativeUrlForTopic(topic);
    if (QFile::exists(filePath))
        return QUrl::fromLocalFile(filePath);

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

QnHelpHandler& QnHelpHandler::instance()
{
    static QnHelpHandler helpHandler;
    return helpHandler;
}
