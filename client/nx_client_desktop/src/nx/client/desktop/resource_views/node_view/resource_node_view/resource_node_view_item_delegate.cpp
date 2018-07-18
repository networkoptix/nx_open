#include "resource_node_view_item_delegate.h"

#include <QtCore/QtMath>
#include <QtGui/QPainter>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QApplication>

#include <client/client_color_types.h>

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_helpers.h>

#include <ui/style/helper.h>
#include <ui/common/text_pixmap_cache.h>
#include <utils/common/scoped_painter_rollback.h>

namespace nx {
namespace client {
namespace desktop {

struct ResourceNodeViewItemDelegate::Private
{
    QnResourceItemColors colors;
    QnTextPixmapCache textPixmapCache;
};

//-------------------------------------------------------------------------------------------------

ResourceNodeViewItemDelegate::ResourceNodeViewItemDelegate(QTreeView* owner, QObject* parent):
    base_type(owner, parent),
    d(new Private())
{
}

ResourceNodeViewItemDelegate::~ResourceNodeViewItemDelegate()
{
}

void ResourceNodeViewItemDelegate::paint(
    QPainter *painter,
    const QStyleOptionViewItem &styleOption,
    const QModelIndex &index) const
{
    base_type::paint(painter, styleOption, index);

    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    const auto rootModel = owner()->model();
    const auto sourceIndex = details::getLeafIndex(index, rootModel);
    const auto node = NodeViewModel::nodeFromIndex(sourceIndex);
    if (!node)
        return;

    const auto style = option.widget ? option.widget->style() : QApplication::style();
    const bool checked = helpers::nodeCheckedState(node) != Qt::Unchecked;
    auto mainColor = checked ? d->colors.mainTextSelected : d->colors.mainText;
    if (option.features.testFlag(QStyleOptionViewItem::HasCheckIndicator))
    {
        mainColor.setAlphaF(option.palette.color(QPalette::Text).alphaF());
        option.palette.setColor(QPalette::Text, mainColor);
        style->drawControl(QStyle::CE_ItemViewItem, &option, painter, option.widget);
    }

    // TOOD: think towmorrow how to move it to the base class (if we need it?)
    /* Obtain sub-element rectangles: */

    const QIcon::Mode iconMode = checked ? QIcon::Selected : QIcon::Normal;
    const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
    const QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &option, option.widget);

    /* Paint background: */
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    /* Set opacity if disabled: */
    QnScopedPainterOpacityRollback opacityRollback(painter);
    if (!option.state.testFlag(QStyle::State_Enabled))
        painter->setOpacity(painter->opacity() * style::Hints::kDisabledItemOpacity);

    /* Draw icon: */
    if (option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        option.icon.paint(painter, iconRect, option.decorationAlignment, iconMode, QIcon::On);

    /* Draw text: */
    const auto text = helpers::nodeText(node, index.column());
    const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; /* As in Qt */
    const int textEnd = textRect.right() - textPadding + 1;

    static constexpr int kExtraTextMargin = 5;
    QPoint textPos = textRect.topLeft() + QPoint(textPadding, option.fontMetrics.ascent()
        + qCeil((textRect.height() - option.fontMetrics.height()) / 2.0));

    if (textEnd > textPos.x())
    {
        const auto main = d->textPixmapCache.pixmap(text, option.font, mainColor,
            textEnd - textPos.x() + 1, option.textElideMode);

        if (!main.pixmap.isNull())
        {
            painter->drawPixmap(textPos + main.origin, main.pixmap);
            textPos.rx() += main.origin.x() + main.size().width() + kExtraTextMargin;
        }
    }
}

const QnResourceItemColors& ResourceNodeViewItemDelegate::colors() const
{
    return d->colors;
}

void ResourceNodeViewItemDelegate::setColors(const QnResourceItemColors& colors)
{
    d->colors = colors;
}

void ResourceNodeViewItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

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

