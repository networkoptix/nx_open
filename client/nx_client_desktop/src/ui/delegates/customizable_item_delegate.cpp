#include "customizable_item_delegate.h"

void QnCustomizableItemDelegate::setCustomInitStyleOption(InitStyleOption initStyleOption)
{
    m_initStyleOption = initStyleOption;
}

void QnCustomizableItemDelegate::setCustomSizeHint(SizeHint sizeHint)
{
    m_sizeHint = sizeHint;
}

void QnCustomizableItemDelegate::setCustomPaint(Paint paint)
{
    m_paint = paint;
}

void QnCustomizableItemDelegate::paint(
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

void QnCustomizableItemDelegate::basePaint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    base_type::paint(painter, option, index);
}

QSize QnCustomizableItemDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (!m_sizeHint)
        return base_type::sizeHint(option, index);

    QStyleOptionViewItem optionCopy(option);
    initStyleOption(&optionCopy, index);
    return m_sizeHint(optionCopy, index);
}

QSize QnCustomizableItemDelegate::baseSizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    return base_type::sizeHint(option, index);
}

void QnCustomizableItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    if (m_initStyleOption)
        m_initStyleOption(option, index);
}
