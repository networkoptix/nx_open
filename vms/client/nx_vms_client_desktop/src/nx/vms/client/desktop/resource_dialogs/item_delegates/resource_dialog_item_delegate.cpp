// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_dialog_item_delegate.h"

#include <QtCore/QtMath>
#include <QtGui/QPainter>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QApplication>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <ui/common/indents.h>
#include <ui/common/text_pixmap_cache.h>
#include <utils/common/scoped_painter_rollback.h>

#include <client/client_settings.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/resource_views/data/camera_extra_status.h>

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

struct ResourceDialogItemDelegate::Private
{
    QnTextPixmapCache textPixmapCache;
    bool showRecordingIndicator = false;
    bool showExtraInfo = false;
};

//-------------------------------------------------------------------------------------------------

ResourceDialogItemDelegate::ResourceDialogItemDelegate(QObject* parent):
    base_type(parent),
    d(new Private())
{
    d->showExtraInfo = (qnSettings->resourceInfoLevel() == Qn::RI_FullInfo);
}

ResourceDialogItemDelegate::~ResourceDialogItemDelegate()
{
}

QSize ResourceDialogItemDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (isSpacer(index))
        return QSize(0, nx::style::Metrics::kViewSpacerRowHeight);

    if (isSeparator(index))
        return QSize(0, nx::style::Metrics::kViewSeparatorRowHeight);

    return base_type::sizeHint(option, index);
}

void ResourceDialogItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    if (isSpacer(index))
        return;

    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    if (isSeparator(index))
    {
        paintSeparator(painter, styleOption, index);
        return;
    }

    QColor textColor;
    QColor extraTextColor;
    QColor errorTextColor;

    const auto colorData = index.data(Qt::ForegroundRole);
    if (colorData.isValid())
    {
        const auto customColor = colorData.value<QColor>();
        textColor = customColor;
        extraTextColor = customColor;
        errorTextColor = customColor;
    }
    else
    {
        textColor = option.state.testFlag(QStyle::State_Selected)
            ? colorTheme()->color("light7")
            : colorTheme()->color("light10");
        extraTextColor = option.state.testFlag(QStyle::State_Selected)
            ? colorTheme()->color("light13")
            : colorTheme()->color("dark17");
        errorTextColor = colorTheme()->color("red_core");
    }

    paintItemText(painter, option, index, textColor, extraTextColor, errorTextColor);
    paintItemIcon(painter, option, index, QIcon::Normal);
}

bool ResourceDialogItemDelegate::showRecordingIndicator() const
{
    return d->showRecordingIndicator;
}

void ResourceDialogItemDelegate::setShowRecordingIndicator(bool show)
{
    d->showRecordingIndicator = show;
}

bool ResourceDialogItemDelegate::isSeparator(const QModelIndex& index)
{
    return checkNodeType(index, NodeType::separator);
}

bool ResourceDialogItemDelegate::isSpacer(const QModelIndex& index)
{
    return checkNodeType(index, NodeType::spacer);
}

void ResourceDialogItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    // Font options initialization.
    option->font.setWeight(QFont::DemiBold);
    option->fontMetrics = QFontMetrics(option->font);

    option->state.setFlag(QStyle::State_HasFocus, false);

    if (!option->state.testFlag(QStyle::State_Enabled) || isSpacer(index) || isSeparator(index))
        option->state.setFlag(QStyle::State_MouseOver, false);
}

int ResourceDialogItemDelegate::decoratorWidth(const QModelIndex& index) const
{
    return 0;
}

void ResourceDialogItemDelegate::paintItemText(
    QPainter* painter,
    QStyleOptionViewItem& option,
    const QModelIndex& index,
    const QColor& mainColor,
    const QColor& extraColor,
    const QColor& invalidColor) const
{
    const auto style = option.widget ? option.widget->style() : QApplication::style();

    const auto isValidData = index.data(Qn::IsValidResourceRole);
    const bool isValid = isValidData.isNull() || isValidData.toBool();

    auto baseColor = isValid ? mainColor : invalidColor;
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
    const auto text = index.data(Qt::DisplayRole).toString();
    const auto extraText = d->showExtraInfo ? index.data(Qn::ExtraInfoRole).toString() : QString();

    const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; /* As in Qt */
    const int textEnd = textRect.right() - textPadding + 1 - decoratorWidth(index);

    static constexpr int kExtraTextMargin = 5;
    QPoint textPos = textRect.topLeft() + QPoint(textPadding, option.fontMetrics.ascent()
        + qCeil((textRect.height() - option.fontMetrics.height()) / 2.0));

    if (textEnd > textPos.x())
    {
        const auto main = d->textPixmapCache.pixmap(text, option.font, baseColor,
            textEnd - textPos.x() + 1, option.textElideMode);

        if (!main.pixmap.isNull())
        {
            painter->drawPixmap(textPos + main.origin, main.pixmap);
            textPos.rx() += main.origin.x() + main.size().width() + kExtraTextMargin;
        }

        if (textEnd > textPos.x() && !main.elided() && !extraText.isEmpty())
        {
            option.font.setWeight(QFont::Normal);

            const auto extra = d->textPixmapCache.pixmap(extraText, option.font,
                extraColor, textEnd - textPos.x(), option.textElideMode);

            if (!extra.pixmap.isNull())
                painter->drawPixmap(textPos + extra.origin, extra.pixmap);
        }
    }
}

void ResourceDialogItemDelegate::paintItemIcon(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index,
    QIcon::Mode mode) const
{
    if (!option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        return;

    QnScopedPainterOpacityRollback opacityRollback(painter);
    if (!option.state.testFlag(QStyle::State_Enabled))
        painter->setOpacity(painter->opacity() * style::Hints::kDisabledItemOpacity);

    const auto style = option.widget ? option.widget->style() : QApplication::style();
    const QRect iconRect = style->subElementRect(
        QStyle::SE_ItemViewItemDecoration, &option, option.widget);
    option.icon.paint(painter, iconRect, option.decorationAlignment, mode, QIcon::On);

    if (showRecordingIndicator())
        paintRecordingIndicator(painter, iconRect, index);
}

void ResourceDialogItemDelegate::paintRecordingIndicator(
    QPainter* painter,
    const QRect& iconRect,
    const QModelIndex& index) const
{
    NX_ASSERT(index.model());
    if (!index.model())
        return;

    const auto extraStatus = index.data(Qn::CameraExtraStatusRole).value<CameraExtraStatus>();

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

    // Draw recording status icon.
    const auto recordingIcon = QnResourceIconCache::cameraRecordingStatusIcon(extraStatus);
    if (!recordingIcon.isNull() && shiftIconLeft())
        recordingIcon.paint(painter, indicatorRect);
}

void ResourceDialogItemDelegate::paintSeparator(
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

} // namespace nx::vms::client::desktop
