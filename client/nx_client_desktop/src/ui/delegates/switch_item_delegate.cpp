#include "switch_item_delegate.h"

#include <client/client_globals.h>

#include <ui/style/helper.h>
#include <ui/style/nx_style.h>


QnSwitchItemDelegate::QnSwitchItemDelegate(QObject* parent) :
    base_type(parent),
    m_hideDisabledItems(false)
{
}

bool QnSwitchItemDelegate::hideDisabledItems() const
{
    return m_hideDisabledItems;
}

void QnSwitchItemDelegate::setHideDisabledItems(bool hide)
{
    m_hideDisabledItems = hide;
}

void QnSwitchItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (QnNxStyle *nxStyle = QnNxStyle::instance())
    {
        /* Init style option: */
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        /* Draw background and focus marker: */
        opt.features &= ~(QStyleOptionViewItem::HasDisplay | QStyleOptionViewItem::HasDecoration | QStyleOptionViewItem::HasCheckIndicator);
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

QSize QnSwitchItemDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const
{
    static const int kFocusFrameMargin = 2;
    return style::Metrics::kStandaloneSwitchSize + QSize(style::Metrics::kSwitchMargin, kFocusFrameMargin) * 2;
}

void QnSwitchItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    //TODO: #vkutin #common Refactor this role
    if (index.data(Qn::DisabledRole).toBool())
        option->state &= ~QStyle::State_Enabled;

    if (!QnNxStyle::instance())
        return;

    if (option->checkState == Qt::Checked)
        option->state |= QStyle::State_On;
    else
        option->state |= QStyle::State_Off;
}
