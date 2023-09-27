// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_bar.h"

#include <memory>
#include <vector>

#include <QtWidgets/QLayout>
#include <QtWidgets/QPushButton>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <ui/common/palette.h>

namespace {

constexpr size_t kFontPixelSize = 14;
constexpr QMargins kMargins{16, 10, 16, 10};

} // namespace

namespace nx::vms::client::desktop {

struct CommonMessageBar::Private
{
    CommonMessageBar* const q;
    QnWordWrappedLabel* const label{new QnWordWrappedLabel(q)};
    QPushButton* const closeButton{
        new QPushButton(qnSkin->icon("banners/close.svg"), QString(), q)};
    QPushButton* icon{new QPushButton(QIcon(), QString(), q)};
    BarDescription::PropertyPointer isEnabledProperty;
};

CommonMessageBar::CommonMessageBar(QWidget* parent, const BarDescription& description):
    base_type(parent),
    d(new Private{.q = this})
{
    setAttribute(Qt::WA_StyledBackground, true);
    horizontalLayout()->addWidget(d->icon);
    horizontalLayout()->addWidget(d->label, 1);
    d->label->setForegroundRole(QPalette::Text);
    d->label->label()->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    d->label->setText(QString());

    connect(d->label, &QnWordWrappedLabel::linkActivated, this, &CommonMessageBar::linkActivated);

    horizontalLayout()->addWidget(d->closeButton, 0, Qt::AlignTop);
    d->closeButton->setFlat(true);
    d->closeButton->setFixedSize(20, 20);
    d->closeButton->setToolTip(tr("Close"));
    d->closeButton->setFocusPolicy(Qt::NoFocus);
    d->closeButton->hide();

    d->icon->setFixedSize(20, 20);
    d->icon->setFocusPolicy(Qt::NoFocus);
    d->icon->setFlat(true);

    connect(d->closeButton, &QPushButton::clicked, this, &CommonMessageBar::closeClicked);
    connect(d->closeButton, &QPushButton::clicked, this, &CommonMessageBar::hide);
    connect(
        d->closeButton, &QPushButton::clicked, this, &CommonMessageBar::hideInFuture);

    init(description);
}

CommonMessageBar::~CommonMessageBar()
{
    // Required here for forward-declared scoped pointer destruction.
}

QString CommonMessageBar::text() const
{
    return d->label->text();
}

void CommonMessageBar::setOpenExternalLinks(bool open)
{
    d->label->setOpenExternalLinks(open);
}

void CommonMessageBar::setText(const QString& text)
{
    if (text == this->text())
        return;

    d->label->setText(text);

    updateVisibility();
}

void CommonMessageBar::updateVisibility()
{
    const bool isHidden =
        d->label->text().isEmpty() || (d->isEnabledProperty && !d->isEnabledProperty->value());
    setDisplayed(!isHidden);
}

void CommonMessageBar::hideInFuture()
{
    if (d->isEnabledProperty)
        d->isEnabledProperty->setValue(false);
}

void CommonMessageBar::init(const BarDescription& barDescription)
{
    d->closeButton->setVisible(barDescription.isClosable || !barDescription.isEnabledProperty.isNull());
    setPaletteColor(this, QPalette::WindowText, core::colorTheme()->color("light4"));

    auto font = d->label->font();
    font.setWeight(QFont::Medium);
    font.setPixelSize(kFontPixelSize);
    d->label->setFont(font);
    verticalLayout()->setContentsMargins(kMargins);

    QString backgroundColor;
    QString frameColor;
    QIcon icon;
    switch (barDescription.level)
    {
        case BarDescription::BarLevel::Info:
            backgroundColor = "attention.blue_bg";
            frameColor = "attention.blue_dark";
            icon = qnSkin->icon("banners/info.svg");
            break;
        case BarDescription::BarLevel::Warning:
            backgroundColor = "attention.yellow_bg";
            frameColor = "yellow_d1";
            icon = qnSkin->icon("banners/warning.svg");
            break;
        case BarDescription::BarLevel::Error:
            backgroundColor = "attention.red_bg";
            frameColor = "red_d1";
            icon = qnSkin->icon("banners/error.svg");
            break;
    }

    setPaletteColor(this, QPalette::Window, core::colorTheme()->color(backgroundColor));
    setPaletteColor(this, QPalette::Dark, core::colorTheme()->color(frameColor));
    d->icon->setIcon(icon);

    setOpenExternalLinks(barDescription.isOpenExternalLinks);

    d->isEnabledProperty = barDescription.isEnabledProperty;

    setText(barDescription.text);

    if (barDescription.isMultiLine)
    {
        d->label->setAutoFillBackground(true);
        d->label->setContentsMargins(style::Metrics::kDefaultTopLevelMargin,
            style::Metrics::kStandardPadding,
            style::Metrics::kDefaultTopLevelMargin,
            style::Metrics::kStandardPadding);
    }

    for (const auto& row: barDescription.additionalRows)
    {
        verticalLayout()->addLayout(row);
    }

    updateVisibility();
}

struct MessageBarBlock::Private
{
    using MessageBarStorage = std::vector<std::unique_ptr<CommonMessageBar>>;
    std::vector<BarDescription> barsDescriptions;
    MessageBarStorage bars;
};

MessageBarBlock::MessageBarBlock(QWidget* parent):
    base_type(parent),
    d(new Private())
{
    auto layout = new QVBoxLayout;
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->setContentsMargins({});
    layout->setSpacing(1);
    setLayout(layout);
}

MessageBarBlock::~MessageBarBlock()
{
}

void MessageBarBlock::setMessageBars(const std::vector<BarDescription>& descs)
{
    if (descs == d->barsDescriptions)
        return;

    // Clear previous banners.
    d->bars.clear();

    // Set new ones.
    for (auto& desc: descs)
    {
        auto bar = std::make_unique<CommonMessageBar>(nullptr, desc);
        layout()->addWidget(bar.get());
        d->bars.push_back(std::move(bar));
    }

    d->barsDescriptions = descs;
}

} // namespace nx::vms::client::desktop
