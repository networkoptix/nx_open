// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "help_handler.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtGui/QCursor>
#include <QtGui/QDesktopServices>
#include <QtGui/QHelpEvent>
#include <QtQuick/QQuickWindow>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QWhatsThis>
#include <QtWidgets/QWidget>

#include <nx/build_info.h>
#include <nx/utils/external_resources.h>
#include <nx/utils/log/log.h>

#include "help_dialog.h"
#include "help_topic_accessor.h"

namespace nx::vms::client::desktop {


class HelpDialog;
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
    openHelpTopic(topic);
}

void HelpHandler::openHelpTopic(int topic)
{
    openHelpTopic(static_cast<HelpTopic::Id>(topic));
}

void HelpHandler::openHelpTopic(HelpTopic::Id topic)
{
    if (nx::build_info::isLinux())
    {
        NX_DEBUG(NX_SCOPE_TAG, "Opening help topic %1 in the internal browser", topic);

        instance().openHelpTopicInternal(topic);
    }
    else
    {
        const auto url = urlForTopic(topic);

        if (url.isEmpty())
            return;

        NX_DEBUG(NX_SCOPE_TAG, "Opening help topic %1 with url %2", topic, url);

        QDesktopServices::openUrl(url);
    }
}

QUrl HelpHandler::urlForTopic(HelpTopic::Id topic)
{
    const auto helpRoot = nx::utils::externalResourcesDirectory().absolutePath() + "/help/";
    QString filePath = helpRoot + HelpTopic::relativeUrlForTopic(topic);

    if (QFile::exists(filePath))
        return QUrl::fromLocalFile(filePath);

    NX_WARNING(NX_SCOPE_TAG, "Unable to find help file %1 for topic %2", filePath, topic);

    return QUrl();
}

void HelpHandler::openHelpTopicInternal(HelpTopic::Id topic)
{
    if (!m_helpDialog)
        m_helpDialog.reset(new HelpDialog());

    m_helpDialog->setUrl(urlForTopic(topic).toString());

    if (m_helpDialog->isVisible())
        m_helpDialog->setFocus();
    else
        m_helpDialog->show();
}

bool HelpHandler::eventFilter(QObject* watched, QEvent* event)
{
    auto widget = qobject_cast<QWidget*>(watched);

    if (widget)
    {
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

                int topicId = HelpTopicAccessor::helpTopicAt(widget, e->pos(), /* bubbleUp */ true);

                NX_DEBUG(this, "WhatsThis event: with widget %1, position: %2, help topic: %3",
                    watched, e->pos(), topicId);

                if (topicId != HelpTopic::Id::Empty)
                {
                    event->accept();

                    setHelpTopic(topicId);

                    QWhatsThis::leaveWhatsThisMode();
                    return true;
                }

                return false;
            }
        }
    }

    if (QWhatsThis::inWhatsThisMode() && event->type() == QEvent::MouseButtonPress)
    {
        auto e = static_cast<QMouseEvent*>(event);

        if (e->button() == Qt::RightButton || e->button() == Qt::LeftButton)
        {
            if (e->button() == Qt::LeftButton)
            {
                if (auto window = qobject_cast<QQuickWindow*>(watched))
                {
                    const auto topicId = HelpTopicAccessor::helpTopicAt(window, e->pos());

                    // Qt Quick does not handle WhatsThis event, so we need to simulate its behavior.
                    if (topicId != HelpTopic::Id::Empty)
                        setHelpTopic(topicId);
                }
            }

            event->accept();
            QWhatsThis::leaveWhatsThisMode();
            return true;
        }

    }

    return false;
}

HelpHandler& HelpHandler::instance()
{
    static HelpHandler helpHandler;
    return helpHandler;
}

} // namespace nx::vms::client::desktop
