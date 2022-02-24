// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_dialog_item_delegate_base.h"

#include <QtGui/QPainter>

#include <client/client_globals.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/common/indents.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {

using NodeType = nx::vms::client::desktop::ResourceTree::NodeType;

bool checkNodeType(const QModelIndex& index, NodeType nodeType)
{
    if (!index.isValid())
        return false;

    const auto nodeTypeData = index.siblingAtColumn(0).data(Qn::NodeTypeRole);
    if (nodeTypeData.isNull())
        return false;

    return nodeTypeData.value<NodeType>() == nodeType;
}

} // namespace

namespace nx::vms::client::desktop {

ResourceDialogItemDelegateBase::ResourceDialogItemDelegateBase(QObject* parent):
    base_type(parent)
{
}

bool ResourceDialogItemDelegateBase::isSpacer(const QModelIndex& index)
{
    return checkNodeType(index, NodeType::spacer);
}

bool ResourceDialogItemDelegateBase::isSeparator(const QModelIndex& index)
{
    return checkNodeType(index, NodeType::separator);
}

QSize ResourceDialogItemDelegateBase::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (isSpacer(index))
        return QSize(0, nx::style::Metrics::kViewSpacerRowHeight);

    if (isSeparator(index))
        return QSize(0, nx::style::Metrics::kViewSeparatorRowHeight);

    return base_type::sizeHint(option, index);
}

void ResourceDialogItemDelegateBase::paint(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    if (isSpacer(index))
        return;
    else if (isSeparator(index))
        paintSeparator(painter, styleOption, index);
    else
        paintContents(painter, styleOption, index);
}

void ResourceDialogItemDelegateBase::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    // Keyboard focus decoration is never displayed across client item views.
    option->state.setFlag(QStyle::State_HasFocus, false);

    // No hover interactions for spacers, separator and disabled items.
    if (!option->state.testFlag(QStyle::State_Enabled) || isSpacer(index) || isSeparator(index))
        option->state.setFlag(QStyle::State_MouseOver, false);
}

void ResourceDialogItemDelegateBase::paintSeparator(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    QnIndents indents;
    if (option.viewItemPosition == QStyleOptionViewItem::Beginning
        || option.viewItemPosition == QStyleOptionViewItem::OnlyOne)
    {
        indents.setLeft(nx::style::Metrics::kDefaultTopLevelMargin);
    }
    if (option.viewItemPosition == QStyleOptionViewItem::End
        || option.viewItemPosition == QStyleOptionViewItem::OnlyOne)
    {
        indents.setRight(nx::style::Metrics::kDefaultTopLevelMargin);
    }
    auto point1 = option.rect.center();
    point1.setX(option.rect.left() + indents.left());

    auto point2 = option.rect.center();
    point2.setX(option.rect.right() - indents.right());

    const QColor separatorColor = colorTheme()->color("dark8");
    QPen pen(separatorColor, 1.0, Qt::SolidLine);

    QnScopedPainterPenRollback penRollback(painter, pen);
    painter->drawLine(point1, point2);
}

void ResourceDialogItemDelegateBase::paintContents(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QStyledItemDelegate::paint(painter, styleOption, index);
}

} // namespace nx::vms::client::desktop
