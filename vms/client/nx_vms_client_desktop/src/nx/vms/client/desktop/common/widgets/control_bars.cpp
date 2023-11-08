// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "control_bars.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// ControlBar

struct ControlBar::Private
{
    ControlBar* const q;
    QWidget* const background{new QWidget(q)};
    QVBoxLayout* const verticalLayout{new QVBoxLayout(background)};
    QHBoxLayout* const horizontalLayout{new QHBoxLayout()};
    bool retainSpaceWhenNotDisplayed = false;

    void updateVisibility()
    {
        const bool hidden = !retainSpaceWhenNotDisplayed && background->isHidden();
        if (hidden == q->isHidden())
            return;

        q->setHidden(hidden);

        if (q->parentWidget() && q->parentWidget()->layout() && q->parentWidget()->isVisible())
            q->parentWidget()->layout()->activate();
    }
};

ControlBar::ControlBar(QWidget* parent):
    base_type(parent),
    d(new Private{.q = this})
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    setPaletteColor(this, QPalette::Window, colorTheme()->color("brand_d5"));
    setPaletteColor(this, QPalette::Link, colorTheme()->color("light4"));
    setPaletteColor(this, QPalette::LinkVisited, colorTheme()->color("light1"));

    d->background->setAutoFillBackground(true);
    d->background->setHidden(true);

    d->verticalLayout->addLayout(d->horizontalLayout);
    d->horizontalLayout->setContentsMargins(0, 0, 0, 0);

    d->verticalLayout->setContentsMargins(
        style::Metrics::kDefaultTopLevelMargin, style::Metrics::kStandardPadding,
        style::Metrics::kDefaultTopLevelMargin, style::Metrics::kStandardPadding);

    WidgetUtils::setRetainSizeWhenHidden(d->background, true);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(d->background);

    setHidden(!d->retainSpaceWhenNotDisplayed);
}

ControlBar::~ControlBar()
{
    // Required here for forward-declared scoped pointer destruction.
}

bool ControlBar::isDisplayed() const
{
    return !d->background->isHidden();
}

void ControlBar::setDisplayed(bool value)
{
    if (isDisplayed() == value)
        return;

    d->background->setVisible(value);
    d->updateVisibility();
}

bool ControlBar::retainSpaceWhenNotDisplayed() const
{
    return d->retainSpaceWhenNotDisplayed;
}

void ControlBar::setRetainSpaceWhenNotDisplayed(bool value)
{
    if (d->retainSpaceWhenNotDisplayed == value)
        return;

    d->retainSpaceWhenNotDisplayed = value;
    d->updateVisibility();
}

QVBoxLayout* ControlBar::verticalLayout() const
{
    return d->verticalLayout;
}

QHBoxLayout* ControlBar::horizontalLayout() const
{
    return d->horizontalLayout;
}

// -----------------------------------------------------------------------------------------------
// MessageBar

struct MessageBar::Private
{
    MessageBar* const q;
    QnWordWrappedLabel* const label{new QnWordWrappedLabel(q)};
    QHBoxLayout* const overlayLayout{new QHBoxLayout(label)};
};

MessageBar::MessageBar(QWidget* parent):
    base_type(parent),
    d(new Private{.q = this})
{
    horizontalLayout()->addWidget(d->label);
    d->label->setForegroundRole(QPalette::Text);
    d->label->label()->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    d->label->setText(QString());
    setOpenExternalLinks(true);

    connect(d->label, &QnWordWrappedLabel::linkActivated,
        this, &MessageBar::linkActivated);
}

MessageBar::~MessageBar()
{
    // Required here for forward-declared scoped pointer destruction.
}

QString MessageBar::text() const
{
    return d->label->text();
}

void MessageBar::setText(const QString& text)
{
    if (text == this->text())
        return;

    d->label->setText(text);
    setDisplayed(!text.isEmpty());
}

void MessageBar::setOpenExternalLinks(bool open)
{
    d->label->setOpenExternalLinks(open);
}

QHBoxLayout* MessageBar::overlayLayout() const
{
    return d->overlayLayout;
}

QnWordWrappedLabel* MessageBar::label() const
{
    return d->label;
}

// ------------------------------------------------------------------------------------------------
// AlertBar

AlertBar::AlertBar(QWidget* parent):
    base_type(parent)
{
    setPaletteColor(this, QPalette::Base, colorTheme()->color("dialog.alertBar")); // This is required when drawing alert bars on the scene.
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
