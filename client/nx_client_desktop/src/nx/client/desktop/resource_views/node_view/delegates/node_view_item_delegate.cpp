#include "node_view_item_delegate.h"

#include <QtWidgets/QTreeView>

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/detail/node_view_helpers.h>

#include <utils/common/scoped_painter_rollback.h>

namespace nx {
namespace client {
namespace desktop {

NodeViewItemDelegate::NodeViewItemDelegate(QTreeView* owner, QObject* parent):
    base_type(parent),
    m_owner(owner)
{
}

const QTreeView* NodeViewItemDelegate::owner() const
{
    return m_owner;
}

void NodeViewItemDelegate::paint(
    QPainter *painter,
    const QStyleOptionViewItem &styleOption,
    const QModelIndex &index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    if (option.features == QStyleOptionViewItem::None)
    {
        int y = option.rect.top() + option.rect.height() / 2;
        QnScopedPainterPenRollback penRollback(painter, option.palette.color(QPalette::Midlight));
        painter->drawLine(0, y, option.rect.right(), y);
    }
    else
    {
        base_type::paint(painter, option, index);
    }
}

void NodeViewItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    const auto sourceIndex = nx::client::desktop::detail::getSourceIndex(index, m_owner->model());
    const auto node = NodeViewModel::nodeFromIndex(sourceIndex);
    if (!node)
        return;

    const bool isSeparator = node->data(node_view::nameColumn, node_view::separatorRole).toBool();

    /* Init font options: */
    option->font.setWeight(QFont::DemiBold);
    option->fontMetrics = QFontMetrics(option->font);

    /* Save default decoration size: */
    QSize defaultDecorationSize = option->decorationSize;

    /* If icon size is explicitly specified in itemview, decorationSize already holds that value.
     * If icon size is not specified in itemview, set decorationSize to something big.
     * It will be treated as maximal allowed size: */

//    auto view = qobject_cast<const QAbstractItemView*>(option->widget);
//    if ((!view || !view->iconSize().isValid()) && m_fixedHeight > 0)
//        option->decorationSize.setHeight(qMin(defaultDecorationSize.height(), m_fixedHeight));

    /* Call inherited implementation.
     * When it configures item icon, it sets decorationSize to actual icon size: */
    base_type::initStyleOption(option, index);

    /* But if the item has no icon, restore decoration size to saved default: */
    if (option->icon.isNull())
        option->decorationSize = defaultDecorationSize;

    if (isSeparator)
        option->features = QStyleOptionViewItem::None;
    else
        option->features |= QStyleOptionViewItem::HasDisplay;

    if (!option->state.testFlag(QStyle::State_Enabled))
        option->state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);

//    // Determine validity.
//    if (m_options.testFlag(ValidateOnlyChecked))
//    {
//        const auto checkState = rowCheckState(index);
//        if (checkState.canConvert<int>() && checkState.toInt() == Qt::Unchecked)
//            return;
//    }

//    const auto isValid = index.data(Qn::ValidRole);
//    if (isValid.canConvert<bool>() && !isValid.toBool())
//        option->state |= State_Error;

//    const auto validationState = index.data(Qn::ValidationStateRole);
//    if (validationState.canConvert<QValidator::State>()
//        && validationState.value<QValidator::State>() == QValidator::Invalid)
//    {
//        option->state |= State_Error;
//    }
}

} // namespace desktop
} // namespace client
} // namespace nx
