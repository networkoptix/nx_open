#include "resource_node_view_item_delegate.h"

#include "../resource_view_node_helpers.h"
#include "../../details/node/view_node_helpers.h"

#include <QtCore/QtMath>
#include <QtGui/QPainter>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QApplication>

#include <ui/style/helper.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/common/text_pixmap_cache.h>
#include <utils/common/scoped_painter_rollback.h>
#include <client/client_color_types.h>
#include <nx/vms/client/desktop/resource_views/data/camera_extra_status.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

struct ResourceNodeViewItemDelegate::Private
{
    QnResourceItemColors colors;
    QnTextPixmapCache textPixmapCache;
    bool showRecordingIndicator = false;
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
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
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

void ResourceNodeViewItemDelegate::setShowRecordingIndicator(bool show)
{
    d->showRecordingIndicator = show;
}

bool ResourceNodeViewItemDelegate::getShowRecordingIndicator() const
{
    return d->showRecordingIndicator;
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
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index,
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
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index,
    QIcon::Mode mode) const
{
    if (!option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        return;

    const auto style = option.widget ? option.widget->style() : QApplication::style();
    const QRect iconRect = style->subElementRect(
        QStyle::SE_ItemViewItemDecoration, &option, option.widget);
    option.icon.paint(painter, iconRect, option.decorationAlignment, mode, QIcon::On);

    if (getShowRecordingIndicator())
        paintRecordingIndicator(painter, iconRect, index);
}

void ResourceNodeViewItemDelegate::paintRecordingIndicator(
    QPainter* painter,
    const QRect& iconRect,
    const QModelIndex& index) const
{
    NX_ASSERT(index.model());
    if (!index.model())
        return;

    const auto extraStatus =
        index.data(ResourceNodeDataRole::cameraExtraStatusRole).value<CameraExtraStatus>();

    if (extraStatus == CameraExtraStatus())
        return;

    const auto nodeHasChildren =
        [](const QModelIndex& index){ return index.model()->rowCount(index) > 0; };

    QRect indicatorRect(iconRect);

    // Check if there are too much icons for this indentation level.
    const auto shiftIconLeft =
        [&indicatorRect]
        {
            indicatorRect.moveLeft(indicatorRect.left() - style::Metrics::kDefaultIconSize);
            return indicatorRect.left() >= 0;
        };

    // Leave space for expand/collapse control if needed.
    if (nodeHasChildren(index))
        shiftIconLeft();

    // Draw "recording" or "scheduled" icon.
    if (extraStatus.testFlag(CameraExtraStatusFlag::recording)
        || extraStatus.testFlag(CameraExtraStatusFlag::scheduled))
    {
        if (!shiftIconLeft())
            return;

        const auto icon = extraStatus.testFlag(CameraExtraStatusFlag::recording)
            ? qnSkin->icon("tree/recording.png")
            : qnSkin->icon("tree/scheduled.png");
        icon.paint(painter, indicatorRect);
    }
}

} // namespace node_view
} // namespace nx::vms::client::desktop
