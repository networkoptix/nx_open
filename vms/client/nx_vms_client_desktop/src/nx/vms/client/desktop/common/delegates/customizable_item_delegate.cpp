#include "customizable_item_delegate.h"

namespace nx::vms::client::desktop {

void CustomizableItemDelegate::setCustomInitStyleOption(InitStyleOption initStyleOption)
{
    m_initStyleOption = initStyleOption;
}

void CustomizableItemDelegate::setCustomSizeHint(SizeHint sizeHint)
{
    m_sizeHint = sizeHint;
}

void CustomizableItemDelegate::setCustomPaint(Paint paint)
{
    m_paint = paint;
}

void CustomizableItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (!m_paint)
    {
        base_type::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem optionCopy(option);
    initStyleOption(&optionCopy, index);
    m_paint(painter, optionCopy, index);
}

void CustomizableItemDelegate::basePaint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    base_type::paint(painter, option, index);
}

QSize CustomizableItemDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (!m_sizeHint)
        return base_type::sizeHint(option, index);

    QStyleOptionViewItem optionCopy(option);
    initStyleOption(&optionCopy, index);
    return m_sizeHint(optionCopy, index);
}

QSize CustomizableItemDelegate::baseSizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    return base_type::sizeHint(option, index);
}

void CustomizableItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    if (m_initStyleOption)
        m_initStyleOption(option, index);
}

} // namespace nx::vms::client::desktop
