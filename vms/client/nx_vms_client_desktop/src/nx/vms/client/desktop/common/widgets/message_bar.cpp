// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_bar.h"

#include <memory>
#include <vector>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qpushbutton.h>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/skin/svg_icon_colorer.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <ui/common/palette.h>

#include "nx/vms/client/desktop/common/widgets/control_bars.h"

namespace {

static const QString kCrossColor = "#E1E7EA";
static const QString kErrorIconColor = "#E1E7EA";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kTextButtonColors = {
    {QIcon::Normal, {{kCrossColor, "light4"}}}, {QIcon::Active, {{kCrossColor, "light3"}}}};
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kErrorTextButtonColors = {
    {QIcon::Normal, {{kCrossColor, "dark4"}}}, {QIcon::Active, {{kCrossColor, "dark5"}}}};
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kErrorIconColors = {
    {QIcon::Normal, {{kErrorIconColor, "dark4"}}}};


} // namespace

namespace nx::vms::client::desktop {

struct CommonMessageBar::Private
{
    CommonMessageBar* const q;
    QnWordWrappedLabel* const label{new QnWordWrappedLabel(q)};
    QPushButton* const closeButton{
        new QPushButton(qnSkin->icon("banners/close.svg", kTextButtonColors), QString(), q)};
    QPushButton* icon{new QPushButton(QIcon(), QString(), q)};
    BarDescription::PropertyPointer isEnabledProperty;
    QHBoxLayout* buttonsHorizontalLayout;
};

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
    {
        for (auto& bar: d->bars)
            bar->updateVisibility();
        return;
    }

    // Clear previous banners.
    d->bars.clear();

    // Set new ones.
    for (auto& desc: descs)
    {
        auto bar = std::make_unique<CommonMessageBar>(this, desc);
        layout()->addWidget(bar.get());
        d->bars.push_back(std::move(bar));
    }

    d->barsDescriptions = descs;
}

CommonMessageBar::CommonMessageBar(QWidget* parent, const BarDescription& description):
    base_type(parent),
    d(new Private{.q = this})
{
    setAttribute(Qt::WA_StyledBackground, true);

    mainLayout()->insertWidget(0, d->icon, {}, Qt::AlignmentFlag::AlignTop);
    verticalLayout()->setContentsMargins({});

    horizontalLayout()->addWidget(d->label, 1);
    d->label->setForegroundRole(QPalette::Text);
    d->label->label()->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    d->label->setText(QString());

    connect(d->label, &QnWordWrappedLabel::linkActivated, this, &CommonMessageBar::linkActivated);

    mainLayout()->insertWidget(2, d->closeButton, 0, Qt::AlignTop);
    d->closeButton->setFlat(true);
    d->closeButton->setFixedSize(
        style::Metrics::kDefaultIconSize, style::Metrics::kDefaultIconSize);
    d->closeButton->setToolTip(tr("Close"));
    d->closeButton->setFocusPolicy(Qt::NoFocus);
    d->closeButton->hide();

    d->icon->setFixedSize(style::Metrics::kDefaultIconSize, style::Metrics::kDefaultIconSize);
    d->icon->setFocusPolicy(Qt::NoFocus);
    d->icon->setFlat(true);

    connect(d->closeButton, &QPushButton::clicked, this, &CommonMessageBar::closeClicked);
    connect(d->closeButton, &QPushButton::clicked, this, &CommonMessageBar::hide);
    connect(
        d->closeButton, &QPushButton::clicked, this, &CommonMessageBar::hideInFuture);

    init(description);

    d->buttonsHorizontalLayout = new QHBoxLayout;
    d->buttonsHorizontalLayout->setSpacing(style::Metrics::kStandardPadding);
    d->buttonsHorizontalLayout->addStretch();
    verticalLayout()->addLayout(d->buttonsHorizontalLayout);
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
    setPaletteColor(this, QPalette::Text,
        (barDescription.level == BarDescription::BarLevel::Error
            ? core::colorTheme()->color("dark4")
            : core::colorTheme()->color("light4")));

    d->closeButton->setIcon(barDescription.level == BarDescription::BarLevel::Error
        ? qnSkin->icon("banners/close.svg", kErrorTextButtonColors)
        : qnSkin->icon("banners/close.svg", kTextButtonColors));

    auto font = d->label->font();
    font.setWeight(QFont::Medium);
    font.setPixelSize(style::Metrics::kBannerLabelFontPixelSize);
    d->label->setFont(font);
    mainLayout()->setContentsMargins(style::Metrics::kMessageBarContentMargins);
    mainLayout()->setSpacing(style::Metrics::kStandardPadding);

    QString backgroundColor;
    QIcon icon;
    switch (barDescription.level)
    {
        case BarDescription::BarLevel::Info:
            backgroundColor = "dark9";
            icon = qnSkin->icon("banners/info.svg");
            break;
        case BarDescription::BarLevel::Warning:
            backgroundColor = "dark9";
            icon = qnSkin->icon("banners/warning.svg");
            break;
        case BarDescription::BarLevel::Error:
            backgroundColor = "red_d3";
            icon = qnSkin->icon("banners/error.svg", kErrorIconColors);
            break;
    }

    setPaletteColor(this, QPalette::Window, core::colorTheme()->color(backgroundColor));
    setPaletteColor(this, QPalette::Dark, {}); // No frame color is used for banners.
    d->icon->setIcon(icon);

    setOpenExternalLinks(barDescription.isOpenExternalLinks);
    d->isEnabledProperty = barDescription.isEnabledProperty;

    setText(barDescription.text);
    updateVisibility();
}

QPushButton* CommonMessageBar::addButton(const QString& text, const QString& iconPath)
{
    QPushButton* button = new QPushButton(this);
    button->setText(text);
    button->setIcon(qnSkin->icon(iconPath, kTextButtonColors));
    addButton(button);
    return button;
}

void CommonMessageBar::addButton(QPushButton* button)
{
    button->setFlat(true);
    setTextButtonWithBackgroundStyle(button);

    // Layout's last element is a Stretch that's added in the constructor.
    d->buttonsHorizontalLayout->insertWidget(d->buttonsHorizontalLayout->count() - 1, button);
}

} // namespace nx::vms::client::desktop
