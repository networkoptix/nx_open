// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "switch_item_delegate.h"

#include <client/client_globals.h>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>

namespace nx::vms::client::desktop {

SwitchItemDelegate::SwitchItemDelegate(QObject* parent): base_type(parent)
{
}

bool SwitchItemDelegate::hideDisabledItems() const
{
    return m_hideDisabledItems;
}

void SwitchItemDelegate::setHideDisabledItems(bool hide)
{
    m_hideDisabledItems = hide;
}

void SwitchItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (auto nxStyle = Style::instance())
    {
        /* Init style option: */
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        /* Draw background and focus marker: */
        opt.features &= ~(QStyleOptionViewItem::HasDisplay
            | QStyleOptionViewItem::HasDecoration
            | QStyleOptionViewItem::HasCheckIndicator);
        nxStyle->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        /* Hide disabled, if needed: */
        if (m_hideDisabledItems && !opt.state.testFlag(QStyle::State_Enabled))
            return;

        /* Draw switch without its own focus marker: */
        opt.state &= ~QStyle::State_HasFocus;
        nxStyle->drawSwitch(painter, &opt, opt.widget);
    }
    else
    {
        base_type::paint(painter, option, index);
    }
}

QSize SwitchItemDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
    const QModelIndex& /*index*/) const
{
    static const int kFocusFrameMargin = 2;
    return style::Metrics::kStandaloneSwitchSize
        + QSize(style::Metrics::kSwitchMargin, kFocusFrameMargin) * 2;
}

void SwitchItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    // TODO: #vkutin Refactor this role.
    if (index.data(Qn::DisabledRole).toBool())
        option->state &= ~QStyle::State_Enabled;

    if (!Style::instance())
        return;

    if (option->checkState == Qt::Checked)
        option->state |= QStyle::State_On;
    else
        option->state |= QStyle::State_Off;
}

} // namespace nx::vms::client::desktop
