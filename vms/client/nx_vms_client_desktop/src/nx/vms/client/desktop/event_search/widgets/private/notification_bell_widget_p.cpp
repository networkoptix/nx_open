// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_bell_widget_p.h"

#include <chrono>

#include <QtGui/QPainter>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/skin.h>

using namespace std::chrono;

namespace nx::vms::client::desktop {

namespace {

const core::SvgIconColorer::ThemeSubstitutions kTabIconSubstitutions = {
    {QnIcon::Disabled, {.primary = "dark14"}},
    {QnIcon::Selected, {.primary = "light4"}},
    {QnIcon::Active, {.primary = "brand_core"}},
    {QnIcon::Normal, {.primary = "light10"}}};
}

NotificationBellWidget::NotificationBellWidget(QWidget* parent):
    QWidget(parent)
{
    m_swingTimer.setInterval(500ms);
    connect(&m_swingTimer, &QTimer::timeout, this,
        [this]
        {
            if (m_widgetState == State::left)
                m_widgetState = State::right;
            else if (m_widgetState == State::right)
                m_widgetState = State::left;
            else
                NX_ASSERT(false, "Unexpected current icon value");

            updateIconLabel();
        });

    m_normalIcon = qnSkin->icon("events/tabs/notifications.svg", kTabIconSubstitutions);
    m_rightIcon = qnSkin->icon("events/tabs/notifications_green.svg");

    setFixedSize(calculateSize());

    QVBoxLayout* verticalLayout = new QVBoxLayout();
    setLayout(verticalLayout);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    verticalLayout->addWidget(m_iconLabel, 0, Qt::AlignCenter);

    updateIconLabel();
}

NotificationBellWidget::~NotificationBellWidget()
{
}

void NotificationBellWidget::setAlarmState(bool enabled)
{
    if (enabled)
    {
        if (m_widgetState == State::normal)
        {
            m_widgetState = State::left;
            updateIconLabel();
            m_swingTimer.start();
        }
    }
    else
    {
        m_widgetState = State::normal;
        updateIconLabel();
        m_swingTimer.stop();
    }
}

void NotificationBellWidget::setIconMode(QIcon::Mode state)
{
    if (m_state == state)
        return;

    m_state = state;
    updateIconLabel();
}

void NotificationBellWidget::updateIconLabel()
{
    QPixmap pixmap;

    switch(m_widgetState)
    {
        case State::normal:
        {
            const auto size = qnSkin->maximumSize(m_normalIcon);
            pixmap = m_normalIcon.pixmap(size, m_state);
            break;
        }
        case State::left:
        {
            const auto size = qnSkin->maximumSize(m_rightIcon);
            pixmap = m_rightIcon.pixmap(size, m_state);
            pixmap = pixmap.transformed(QTransform().scale(-1, 1));
            break;
        }
        case State::right:
        {
            const auto size = qnSkin->maximumSize(m_rightIcon);
            pixmap = m_rightIcon.pixmap(size, m_state);
            break;
        }
        default:
        {
            NX_ASSERT(false, "Unexpected state");
        }
    }

    m_iconLabel->setPixmap(pixmap);
}

QSize NotificationBellWidget::calculateSize() const
{
    const QSize normalSize = qnSkin->maximumSize(m_normalIcon);
    const QSize rightSize = qnSkin->maximumSize(m_rightIcon);

    return {
        std::max({normalSize.width(), rightSize.width()}),
        std::max({normalSize.height(), rightSize.height()})
    };
}

} //namespace nx::vms::client::desktop
