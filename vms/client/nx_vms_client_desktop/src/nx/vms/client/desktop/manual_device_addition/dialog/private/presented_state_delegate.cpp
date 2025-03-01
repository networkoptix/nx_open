// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "presented_state_delegate.h"

#include <QtWidgets/QLayout>
#include <QtWidgets/QPushButton>

#include <nx/utils/switch.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/loading_indicator.h>
#include <nx/vms/client/desktop/manual_device_addition/dialog/private/found_devices_model.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>

namespace {

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {.primary = "light10"}},
    {QIcon::Active, {.primary = "light11"}},
};

NX_DECLARE_COLORIZED_ICON(kSuccessIcon, "20x20/Outline/check.svg", kIconSubstitutions)

using namespace nx::vms::client::desktop;

class CellWidget: public QWidget
{
    using base_type = QWidget;
    Q_DECLARE_TR_FUNCTIONS(CellWidget)

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

    const QString text = nx::utils::switch_(value,
        FoundDevicesModel::limitReachedState, [] { return tr("Canceled"); },
        FoundDevicesModel::alreadyAddedState, [] { return tr("Added"); },
        nx::utils::default_, [] { return tr("Adding"); });

    const auto button = new QPushButton(text, this);
    button->setFlat(true);
    button->setAttribute(Qt::WA_TransparentForMouseEvents);
    button->setFocusPolicy(Qt::NoFocus);

    static constexpr auto kBigStretch = 100;
    const auto currentLayout = new QHBoxLayout(this);
    currentLayout->addStretch(kBigStretch);
    currentLayout->addWidget(button);

    if (value == FoundDevicesModel::alreadyAddedState)
    {
        button->setIcon(qnSkin->icon(kSuccessIcon));
    }
    else if (value == FoundDevicesModel::limitReachedState)
    {
        button->setIcon(QIcon());
    }
    else
    {
        auto indicator = new LoadingIndicator(this);
        connect(indicator, &LoadingIndicator::frameChanged, button,
            [button](const QPixmap& pixmap) { button->setIcon(pixmap); });
    }
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
