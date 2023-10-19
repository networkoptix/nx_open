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
} // namespace nx::vms::client::desktop
