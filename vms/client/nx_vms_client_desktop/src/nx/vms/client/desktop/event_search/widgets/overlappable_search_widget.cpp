// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "overlappable_search_widget.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedLayout>

#include <nx/vms/client/desktop/cross_system/dialogs/cloud_layouts_intro_dialog.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <utils/common/delayed.h>

#include "abstract_search_widget.h"

namespace nx::vms::client::desktop {

namespace {

const auto kPlaceholderSpacing = 16;
const auto kPlaceholderMargin = 24;
const auto kPlaceholderFontSize = 16;
const QString kPlaceholderTextTemplate = "<center><p>%1</p><p><font size='-1'>%2</font></p></center>";

} // namespace

struct OverlappableSearchWidget::Private
{
    AbstractSearchWidget* searchWidget{};
    QStackedLayout* stackedLayout{};
};

OverlappableSearchWidget::OverlappableSearchWidget(
    AbstractSearchWidget* searchWidget,
    QWidget* parent)
    :
    QWidget(parent),
    d(new Private{searchWidget, new QStackedLayout})
{
    d->stackedLayout->addWidget(d->searchWidget);

    auto layout = new QVBoxLayout;
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(kPlaceholderSpacing);
    layout->setContentsMargins(
        kPlaceholderMargin,
        kPlaceholderMargin,
        kPlaceholderMargin,
        kPlaceholderMargin);

    auto placeholderIcon = new QLabel;
    placeholderIcon->setPixmap(qnSkin->pixmap("misc/future.svg"));
    layout->addWidget(placeholderIcon, 0, Qt::AlignHCenter);

    auto placeholderText = new QLabel;
    placeholderText->setWordWrap(true);
    QFont font;
    font.setPixelSize(kPlaceholderFontSize);
    placeholderText->setProperty(style::Properties::kDontPolishFontProperty, true);
    placeholderText->setFont(font);
    placeholderText->setForegroundRole(QPalette::Mid);
    placeholderText->setText(kPlaceholderTextTemplate.arg(
        tr("Not supported"),
        tr("This tab will be available in future versions")));
    layout->addWidget(placeholderText);

    auto button = new QPushButton;
    button->setText(tr("Learn more"));
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    connect(
        button,
        &QPushButton::clicked,
        this,
        []
        {
            CloudLayoutsIntroDialog introDialog(CloudLayoutsIntroDialog::Mode::info);
            introDialog.exec();
        });
    layout->addWidget(button, 0, Qt::AlignHCenter);

    auto overlapWidget = new QWidget;
    overlapWidget->setLayout(layout);

    d->stackedLayout->addWidget(overlapWidget);

    setLayout(d->stackedLayout);
}

OverlappableSearchWidget::~OverlappableSearchWidget() = default;

AbstractSearchWidget* OverlappableSearchWidget::searchWidget() const
{
    return d->searchWidget;
}

void OverlappableSearchWidget::setAppearance(Appearance appearance)
{
    d->stackedLayout->setCurrentIndex(static_cast<int>(appearance));
}

} // namespace nx::vms::client::desktop
