#include "customizable_item_delegate.h"

QnCustomizableItemDelegate::QnCustomizableItemDelegate(QObject* parent):
    base_type(parent),
    m_baseInitStyleOption(
        [this](QStyleOptionViewItem* option, const QModelIndex& index)
        {
            baseInitStyleOption(option, index);
        }),
    m_baseSizeHint(
        [this](const QStyleOptionViewItem& option, const QModelIndex& index) -> QSize
        {
            return base_type::sizeHint(option, index);
        }),
    m_basePaint(
        [this](QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
        {
            base_type::paint(painter, option, index);
        })
{
}

void QnCustomizableItemDelegate::setCustomInitStyleOption(CustomInitStyleOption initStyleOption)
{
    m_initStyleOption = initStyleOption;
}

void QnCustomizableItemDelegate::setCustomSizeHint(CustomSizeHint sizeHint)
{
    m_sizeHint = sizeHint;
}

void QnCustomizableItemDelegate::setCustomPaint(CustomPaint paint)
{
    m_paint = paint;
}

void QnCustomizableItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (m_paint)
        m_paint(m_basePaint, painter, option, index);
    else
        m_basePaint(painter, option, index);
}

QSize QnCustomizableItemDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    return m_sizeHint
        ? m_sizeHint(m_baseSizeHint, option, index)
        : m_baseSizeHint(option, index);
}

void QnCustomizableItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    if (m_initStyleOption)
        m_initStyleOption(m_baseInitStyleOption, option, index);
    else
        m_baseInitStyleOption(option, index);
}

void QnCustomizableItemDelegate::baseInitStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);
}
