// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "help_handler.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtGui/QCursor>
#include <QtGui/QDesktopServices>
#include <QtGui/QHelpEvent>
#include <QtWidgets/QWhatsThis>
#include <QtWidgets/QWidget>

#include <nx/vms/utils/external_resources.h>

#include "help_topic_accessor.h"

namespace nx::vms::client::desktop {

HelpHandler::HelpHandler(QObject* parent):
    QObject(parent)
{
}

HelpHandler::~HelpHandler()
{
}

void HelpHandler::setHelpTopic(int topic)
{
    setHelpTopic(static_cast<HelpTopic::Id>(topic));
}

void HelpHandler::setHelpTopic(HelpTopic::Id topic)
{
    m_topic = topic;
    openHelpTopic(topic);
}

void HelpHandler::openHelpTopic(int topic)
{
    openHelpTopic(static_cast<HelpTopic::Id>(topic));
}

void HelpHandler::openHelpTopic(HelpTopic::Id topic)
{
    QDesktopServices::openUrl(urlForTopic(static_cast<HelpTopic::Id>(topic)));
}

QUrl HelpHandler::urlForTopic(HelpTopic::Id topic)
{
    const auto helpRoot = nx::vms::utils::externalResourcesDirectory().absolutePath() + "/help/";
    QString filePath = helpRoot + HelpTopic::relativeUrlForTopic(topic);
    if (QFile::exists(filePath))
        return QUrl::fromLocalFile(filePath);

    return QUrl();
}

bool HelpHandler::eventFilter(QObject* watched, QEvent* event)
{
    auto widget = qobject_cast<QWidget*>(watched);
    if (!widget)
        return false;

    switch (event->type())
    {
        case QEvent::QueryWhatsThis:
        {
            bool accepted =
                HelpTopicAccessor::helpTopicAt(widget, widget->mapFromGlobal(QCursor::pos()))
                    != HelpTopic::Id::Empty;
            event->setAccepted(accepted);
            return accepted;
        }
        case QEvent::WhatsThis:
        {
            auto e = static_cast<QHelpEvent*>(event);

            int topicId = HelpTopicAccessor::helpTopicAt(widget, e->pos(), true);

            if (topicId != HelpTopic::Id::Empty)
            {
                event->accept();

                setHelpTopic(topicId);

                QWhatsThis::leaveWhatsThisMode();
                return true;
            }

            return false;
        }
        case QEvent::MouseButtonPress:
        {
            auto e = static_cast<QMouseEvent*>(event);

            if (e->button() == Qt::RightButton && QWhatsThis::inWhatsThisMode())
            {
                QWhatsThis::leaveWhatsThisMode();
                return true;
            }

            return false;
        }
        default:
            return false;
    }
}

HelpHandler& HelpHandler::instance()
{
    static HelpHandler helpHandler;
    return helpHandler;
}

} // namespace nx::vms::client::desktop
