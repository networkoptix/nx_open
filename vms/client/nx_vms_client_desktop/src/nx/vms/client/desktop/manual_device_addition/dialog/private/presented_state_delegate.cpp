// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "presented_state_delegate.h"

#include <QtWidgets/QLayout>
#include <QtWidgets/QPushButton>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/manual_device_addition/dialog/private/found_devices_model.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>

namespace {

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {.primary = "light10"}},
    {QIcon::Active, {.primary = "light11"}},
};

NX_DECLARE_COLORIZED_ICON(kSuccessIcon, "20x20/Outline/success.svg", kIconSubstitutions)

using namespace nx::vms::client::desktop;

class CellWidget: public QWidget
{
    using base_type = QWidget;

public:
    CellWidget(QWidget* parent = nullptr);

    void setState(FoundDevicesModel::PresentedState value);
};

CellWidget::CellWidget(QWidget* parent):
    base_type(parent)
{
}

void CellWidget::setState(FoundDevicesModel::PresentedState value)
{
    WidgetUtils::removeLayout(layout());
    if (value == FoundDevicesModel::notPresentedState)
        return;

    const bool addedState = value == FoundDevicesModel::alreadyAddedState;
    const auto text = addedState
        ? PresentedStateDelegate::tr("Added")
        : PresentedStateDelegate::tr("Adding");

    const auto button = new QPushButton(text, this);
    button->setFlat(true);
    button->setAttribute(Qt::WA_TransparentForMouseEvents);
    button->setFocusPolicy(Qt::NoFocus);

    static constexpr auto kBigStretch = 100;
    const auto currentLayout = new QHBoxLayout(this);
    currentLayout->addStretch(kBigStretch);
    currentLayout->addWidget(button);

    if (addedState)
    {
        button->setIcon(qnSkin->icon(kSuccessIcon));
        return;
    }

    const auto movie = qnSkin->newMovie("legacy/loading.gif", button);

    connect(movie, &QMovie::frameChanged, button,
        [button, movie](int /*frameNumber*/)
        {
            button->setIcon(movie->currentPixmap());
        });

    if (movie->loopCount() != -1)
        connect(movie, &QMovie::finished, movie, &QMovie::start);

    movie->start();
}

} // namespace

namespace nx::vms::client::desktop {

PresentedStateDelegate::PresentedStateDelegate(QObject* parent):
    base_type(parent)
{
}

QWidget* PresentedStateDelegate::createEditor(QWidget* parent,
    const QStyleOptionViewItem& /*option*/,
    const QModelIndex& /*index*/) const
{
    return new CellWidget(parent);
}

void PresentedStateDelegate::setEditorData(QWidget *editor,
    const QModelIndex &index) const
{
    if (const auto cell = dynamic_cast<CellWidget*>(editor))
    {
        cell->setState(index.data(FoundDevicesModel::presentedStateRole)
            .value<FoundDevicesModel::PresentedState>());
    }
}

} // namespace nx::vms::client::desktop
