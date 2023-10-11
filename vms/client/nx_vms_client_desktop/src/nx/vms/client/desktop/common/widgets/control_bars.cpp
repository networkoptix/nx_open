// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "control_bars.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QPushButton>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// ControlBar

struct ControlBar::Private
{
    ControlBar* const q;
    CustomPainted<QWidget>* const background{new CustomPainted<QWidget>(q)};
    QHBoxLayout* const mainLayout{new QHBoxLayout(background)};
    QVBoxLayout* const verticalLayout{new QVBoxLayout()};
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

    // Background color.
    setPaletteColor(this, QPalette::Window, core::colorTheme()->color("brand_d5"));

    // Frame color.
    setPaletteColor(this, QPalette::Dark, QColor{});

    // Hyperlink colors.
    setPaletteColor(this, QPalette::Link, core::colorTheme()->color("light4"));
    setPaletteColor(this, QPalette::LinkVisited, core::colorTheme()->color("light1"));

    d->background->setAutoFillBackground(false);
    d->background->setHidden(true);

    d->background->setCustomPaintFunction(
        [this](QPainter* painter, const QStyleOption* option, const QWidget* widget)
        {
            const auto& frameColor = option->palette.color(QPalette::Dark);
            const auto& fillBrush = option->palette.brush(QPalette::Window);
            painter->setPen(frameColor.isValid() ? QPen{frameColor} : QPen{Qt::NoPen});
            painter->setBrush(fillBrush);

            // Anti-aliasing is necessary to prevent integer rounding that causes the borders to
            // have unequal thickness on Windows systems with display scaling larger than 100%. Qt
            // will draw two lines, +/- 0.5 pixels (for pen width == 1) around the given rectangle.
            // That's why we add 0.5 pixels top-left and subtract 0.5 pixels from the right-bottom
            // edge, to contain the drawing entirely inside the widget area. Additional subtraction
            // of 1/2 pixels from the sides is to prevent blurry right and bottom edges on Windows
            // systems with display scaling factor 100%.
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);

            QRectF rect(option->rect);
            auto penWidth = painter->pen().widthF();
            rect.adjust(penWidth / 2, penWidth / 2, -3 * penWidth / 2, -3 * penWidth / 2);

            painter->drawRect(rect);
            painter->restore();

            return true;
        });

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
    mainLayout()->addLayout(verticalLayout());
}

ControlBar::~ControlBar()
{
    // Required here for forward-declared scoped pointer destruction.
}


QHBoxLayout* ControlBar::mainLayout() const
{
    return d->mainLayout;
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
    QPushButton* const closeButton{new QPushButton(
        qnSkin->icon("banners/close.svg"), QString(), q)};
};

MessageBar::MessageBar(QWidget* parent):
    base_type(parent),
    d(new Private{.q = this})
{
    horizontalLayout()->addWidget(d->label, 1);
    d->label->setForegroundRole(QPalette::Text);
    d->label->label()->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    d->label->setText(QString());
    setOpenExternalLinks(true);

    connect(d->label, &QnWordWrappedLabel::linkActivated, this, &MessageBar::linkActivated);

    horizontalLayout()->addWidget(d->closeButton, 0, Qt::AlignTop);
    d->closeButton->setFlat(true);
    d->closeButton->setFixedSize(20, 20);
    d->closeButton->setToolTip(tr("Close"));
    d->closeButton->setFocusPolicy(Qt::NoFocus);
    d->closeButton->hide();

    connect(d->closeButton, &QPushButton::clicked, this, &MessageBar::closeClicked);
    connect(d->closeButton, &QPushButton::clicked, this, &MessageBar::hide);
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

bool MessageBar::closeButtonVisible() const
{
    return d->closeButton->isVisible();
}

void MessageBar::setCloseButtonVisible(bool visible)
{
    d->closeButton->setVisible(visible);
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
    setPaletteColor(this, QPalette::Window, core::colorTheme()->color("dialog.alertBar"));
    setPaletteColor(this, QPalette::WindowText, core::colorTheme()->color("light4"));
}

// ------------------------------------------------------------------------------------------------
// MultilineAlertBar

MultilineAlertBar::MultilineAlertBar(QWidget* parent): base_type(parent)
{
    setPaletteColor(this, QPalette::Window, core::colorTheme()->color("dialog.alertBar"));
    setPaletteColor(this, QPalette::Link, core::colorTheme()->color("light4"));
    setPaletteColor(this, QPalette::LinkVisited, core::colorTheme()->color("light1"));

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
