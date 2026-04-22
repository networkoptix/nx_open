// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "busy_indicator_button.h"

#include <QtWidgets/QStylePainter>
#include <QtWidgets/QStyleOptionButton>

#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>

namespace nx::vms::client::desktop {

BusyIndicatorButton::BusyIndicatorButton(QWidget* parent):
    base_type(parent),
    m_indicator(new BusyIndicatorWidget(this))
{
    m_indicator->setHidden(true);
}

BusyIndicatorWidget* BusyIndicatorButton::indicator() const
{
    return m_indicator;
}

bool BusyIndicatorButton::isBusy() const
{
    return isEnabled() && m_indicator->isVisible();
}

void BusyIndicatorButton::setBusy(bool busy)
{
    setIndicatorVisible(busy);
    setEnabled(!busy);
}

bool BusyIndicatorButton::isIndicatorVisible() const
{
    return !m_indicator->isHidden();
}

void BusyIndicatorButton::setIndicatorVisible(bool show)
{
    m_indicator->setVisible(show);
}

void BusyIndicatorButton::resizeEvent(QResizeEvent* event)
{
    base_type::resizeEvent(event);
    m_indicator->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, m_indicator->size(), rect()));
}

void BusyIndicatorButton::paintEvent(QPaintEvent* /*event*/)
{
    QStyleOptionButton option;
    initStyleOption(&option);

    if (isIndicatorVisible())
    {
        option.text = QString();
        option.icon = QIcon();
    }

    QStylePainter(this).drawControl(QStyle::CE_PushButton, option);
}

void BusyIndicatorButton::changeEvent(QEvent* event)
{
    base_type::changeEvent(event);

    m_indicator->setIndicatorRole(isEnabled() && isDefault()
        ? QPalette::ButtonText
        : QPalette::HighlightedText);
}

} // namespace nx::vms::client::desktop
