// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "overlappable_search_widget.h"

#include <QtWidgets/QStackedLayout>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/cross_system/dialogs/cloud_layouts_intro_dialog.h>
#include <nx/vms/client/desktop/event_search/widgets/abstract_search_widget.h>

#include "abstract_search_widget.h"
#include "placeholder_widget.h"

namespace {
nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kPlaceholdeTheme = {
    {QnIcon::Normal, {.primary = "dark16"}}};

NX_DECLARE_COLORIZED_ICON(kFutureIcon, "64x64/Outline/future.svg", kPlaceholdeTheme)

}  // namespace
namespace nx::vms::client::desktop {

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
    setLayout(d->stackedLayout);

    d->stackedLayout->addWidget(d->searchWidget);
    d->stackedLayout->addWidget(PlaceholderWidget::create(
        qnSkin->icon(kFutureIcon).pixmap(64, 64),
        AbstractSearchWidget::makePlaceholderText(
            tr("Not supported"), tr("This tab will be available in future versions")),
        tr("Learn more"),
        []
        {
            CloudLayoutsIntroDialog introDialog(CloudLayoutsIntroDialog::Mode::info);
            introDialog.exec();
        },
        this
    ));
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
