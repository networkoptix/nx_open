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

    const auto extraColor = checked ? d->colors.extraTextSelected : d->colors.extraText;
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
    const auto extraText = helpers::nodeExtraText(node, index.column());

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

        if (textEnd > textPos.x() && !main.elided() && !extraText.isEmpty())
        {
            option.font.setWeight(QFont::Normal);

            const auto extra = d->textPixmapCache.pixmap(extraText, option.font, extraColor,
                textEnd - textPos.x(), option.textElideMode);

            if (!extra.pixmap.isNull())
                painter->drawPixmap(textPos + extra.origin, extra.pixmap);
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

    if (!option->state.testFlag(QStyle::State_Enabled))
        option->state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);
}

} // namespace desktop
} // namespace client
} // namespace nx

