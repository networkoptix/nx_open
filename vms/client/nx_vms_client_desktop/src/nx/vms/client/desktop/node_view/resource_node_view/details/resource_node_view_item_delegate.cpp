#include "resource_node_view_item_delegate.h"

#include "../resource_view_node_helpers.h"
#include "../../details/node/view_node_helpers.h"

#include <QtCore/QtMath>
#include <QtGui/QPainter>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QApplication>

#include <ui/style/helper.h>
#include <ui/style/globals.h>
#include <ui/common/text_pixmap_cache.h>
#include <utils/common/scoped_painter_rollback.h>
#include <client/client_color_types.h>

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

struct ResourceNodeViewItemDelegate::Private
{
    QnResourceItemColors colors;
    QnTextPixmapCache textPixmapCache;
};

//-------------------------------------------------------------------------------------------------

ResourceNodeViewItemDelegate::ResourceNodeViewItemDelegate(
    QTreeView* owner,
    QObject* parent)
    :
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

    paintItemText(painter, option, index, d->colors.mainText,
        d->colors.extraText, qnGlobals->errorTextColor());
    paintItemIcon(painter, option, index, QIcon::Normal);
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

    // Font options initialization.
    option->font.setWeight(QFont::DemiBold);
    option->fontMetrics = QFontMetrics(option->font);

    if (!option->state.testFlag(QStyle::State_Enabled))
        option->state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);
}

void ResourceNodeViewItemDelegate::paintItemText(
    QPainter *painter,
    const QStyleOptionViewItem &styleOption,
    const QModelIndex &index,
    const QColor& mainColor,
    const QColor& extraColor,
    const QColor& invalidColor) const
{
    auto option = styleOption;
    const auto style = option.widget ? option.widget->style() : QApplication::style();
    auto baseColor = isValidNode(index) ? mainColor : invalidColor;
    if (option.features.testFlag(QStyleOptionViewItem::HasCheckIndicator))
    {
        baseColor.setAlphaF(option.palette.color(QPalette::Text).alphaF());
        option.palette.setColor(QPalette::Text, baseColor);
        style->drawControl(QStyle::CE_ItemViewItem, &option, painter, option.widget);
    }

    // Sub-element rectangles obtaining.
    const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);

    // Background painting.
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    // Sets opacity if disabled.
    QnScopedPainterOpacityRollback opacityRollback(painter);
    if (!option.state.testFlag(QStyle::State_Enabled))
        painter->setOpacity(painter->opacity() * style::Hints::kDisabledItemOpacity);

    // Text drawing.
    const auto nodeText = text(index);
    const auto nodeExtraText = extraText(index);

    const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; /* As in Qt */
    const int textEnd = textRect.right() - textPadding + 1;

    static constexpr int kExtraTextMargin = 5;
    QPoint textPos = textRect.topLeft() + QPoint(textPadding, option.fontMetrics.ascent()
        + qCeil((textRect.height() - option.fontMetrics.height()) / 2.0));

    if (textEnd > textPos.x())
    {
        const auto main = d->textPixmapCache.pixmap(nodeText, option.font, baseColor,
            textEnd - textPos.x() + 1, option.textElideMode);

        if (!main.pixmap.isNull())
        {
            painter->drawPixmap(textPos + main.origin, main.pixmap);
            textPos.rx() += main.origin.x() + main.size().width() + kExtraTextMargin;
        }

        if (textEnd > textPos.x() && !main.elided() && !nodeExtraText.isEmpty())
        {
            option.font.setWeight(QFont::Normal);

            const auto extra = d->textPixmapCache.pixmap(nodeExtraText, option.font,
                extraColor, textEnd - textPos.x(), option.textElideMode);

            if (!extra.pixmap.isNull())
                painter->drawPixmap(textPos + extra.origin, extra.pixmap);
        }
    }
}

void ResourceNodeViewItemDelegate::paintItemIcon(
    QPainter *painter,
    const QStyleOptionViewItem &option,
    const QModelIndex &index,
    QIcon::Mode mode) const
{
    if (!option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        return;

    const auto style = option.widget ? option.widget->style() : QApplication::style();
    const QRect iconRect = style->subElementRect(
        QStyle::SE_ItemViewItemDecoration, &option, option.widget);
        option.icon.paint(painter, iconRect, option.decorationAlignment, mode, QIcon::On);
}

} // namespace node_view
} // namespace nx::vms::client::desktop

