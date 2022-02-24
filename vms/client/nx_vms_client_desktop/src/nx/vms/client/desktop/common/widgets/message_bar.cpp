// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_bar.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// MessageBar

MessageBar::MessageBar(QWidget* parent):
    base_type(parent),
    m_background(new QWidget(this)),
    m_label(new QnWordWrappedLabel(m_background)),
    m_layout(new QVBoxLayout(m_background)),
    m_overlayLayout(new QHBoxLayout(m_label))
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    setPaletteColor(this, QPalette::Window, colorTheme()->color("brand_d5"));
    setPaletteColor(this, QPalette::Link, colorTheme()->color("light4"));
    setPaletteColor(this, QPalette::LinkVisited, colorTheme()->color("light1"));

    m_background->setAutoFillBackground(true);
    m_background->setHidden(true);

    m_layout->addWidget(m_label);

    m_label->setForegroundRole(QPalette::Text);
    m_label->label()->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_label->setText(QString());
    setOpenExternalLinks(true);
    WidgetUtils::setRetainSizeWhenHidden(m_background, true);

    m_background->setContentsMargins(
        style::Metrics::kDefaultTopLevelMargin, style::Metrics::kStandardPadding,
        style::Metrics::kDefaultTopLevelMargin, style::Metrics::kStandardPadding);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_background);

    setHidden(!m_reservedSpace);

    connect(m_label, &QnWordWrappedLabel::linkActivated,
        this, &MessageBar::linkActivated);
}

QString MessageBar::text() const
{
    return m_label->text();
}

void MessageBar::setText(const QString& text)
{
    if (text == this->text())
        return;

    m_label->setText(text);
    m_background->setHidden(text.isEmpty());

    updateVisibility();
}

void MessageBar::setOpenExternalLinks(bool open)
{
    m_label->setOpenExternalLinks(open);
}

bool MessageBar::reservedSpace() const
{
    return m_reservedSpace;
}

void MessageBar::setReservedSpace(bool reservedSpace)
{
    if (m_reservedSpace == reservedSpace)
        return;

    m_reservedSpace = reservedSpace;
    updateVisibility();
}

QVBoxLayout* MessageBar::mainLayout() const
{
    return m_layout;
}

QHBoxLayout* MessageBar::overlayLayout() const
{
    return m_overlayLayout;
}

QnWordWrappedLabel* MessageBar::label() const
{
    return m_label;
}

void MessageBar::updateVisibility()
{
    bool hidden = !m_reservedSpace && m_background->isHidden();
    if (hidden == isHidden())
        return;

    setHidden(hidden);

    if (parentWidget() && parentWidget()->layout() && parentWidget()->isVisible())
        parentWidget()->layout()->activate();
}

// ------------------------------------------------------------------------------------------------
// AlertBar

AlertBar::AlertBar(QWidget* parent):
    base_type(parent)
{
    setPaletteColor(this, QPalette::Window, colorTheme()->color("dialog.alertBar"));
    setPaletteColor(this, QPalette::WindowText, colorTheme()->color("light4"));
}

// ------------------------------------------------------------------------------------------------
// MultilineAlertBar

MultilineAlertBar::MultilineAlertBar(QWidget* parent): base_type(parent)
{
    setPaletteColor(this, QPalette::Window, colorTheme()->color("dialog.alertBar"));
    setPaletteColor(this, QPalette::Link, colorTheme()->color("light4"));
    setPaletteColor(this, QPalette::LinkVisited, colorTheme()->color("light1"));

    label()->setAutoFillBackground(true);
    label()->setForegroundRole(QPalette::Text);

    label()->setContentsMargins(
        style::Metrics::kDefaultTopLevelMargin, style::Metrics::kStandardPadding,
        style::Metrics::kDefaultTopLevelMargin, style::Metrics::kStandardPadding);

    setOpenExternalLinks(true);
}

// ------------------------------------------------------------------------------------------------
// AlertBlock

AlertBlock::AlertBlock(QWidget* parent):
    base_type(parent),
    m_layout(new QVBoxLayout(this))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_layout->setContentsMargins({});
    m_layout->setSpacing(1);
}

void AlertBlock::setAlerts(const QStringList& value)
{
    auto barIter = m_bars.begin();
    for (const auto& text: value)
    {
        if (!NX_ASSERT(!text.isEmpty()))
            continue;

        if (barIter == m_bars.end())
        {
            barIter = m_bars.insert(m_bars.end(), new AlertBar(this));
            m_layout->addWidget(*barIter);
        }

        (*barIter)->setText(text);
        ++barIter;
    }

    for (; barIter != m_bars.end(); ++barIter)
        (*barIter)->setText(QString());

    m_layout->activate();
}

} // namespace nx::vms::client::desktop
