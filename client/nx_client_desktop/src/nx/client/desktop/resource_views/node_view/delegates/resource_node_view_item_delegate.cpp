#include "resource_node_view_item_delegate.h"

#include <QtCore/QtMath>
#include <QtGui/QPainter>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QApplication>

#include <client/client_color_types.h>

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/detail/node_view_helpers.h>

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

void ResourceNodeViewItemDelegate::paint(
    QPainter *painter,
    const QStyleOptionViewItem &styleOption,
    const QModelIndex &index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    const auto topModel = owner()->model();
    const auto sourceIndex = nx::client::desktop::detail::getSourceIndex(index, topModel);
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
    else if (option.features == QStyleOptionViewItem::None)
    {
        int y = option.rect.top() + option.rect.height() / 2;
        QnScopedPainterPenRollback penRollback(painter, option.palette.color(QPalette::Midlight));
        painter->drawLine(0, y, option.rect.right(), y);
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

} // namespace desktop
} // namespace client
} // namespace nx

