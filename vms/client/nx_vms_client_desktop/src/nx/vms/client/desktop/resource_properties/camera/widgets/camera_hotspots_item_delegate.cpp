// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_item_delegate.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>

#include <client/client_globals.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/resource_properties/camera/widgets/camera_hotspots_item_model.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {

using namespace nx::vms::client::desktop;

static constexpr QSize kPaletteCellSize = {16, 16};
static constexpr int kPaletteCellSpacing = 4;
static constexpr int kPaletteCellCornerRadius = 2;

QRect colorPaletteCellRect(int colorIndex, const QRect& viewItemRect)
{
    using nx::style::Metrics;

    const QPoint topLeft(
        colorIndex * (kPaletteCellSize.width() + kPaletteCellSpacing) + Metrics::kStandardPadding,
        (viewItemRect.height() - kPaletteCellSize.height()) / 2);

    return QRect(topLeft, kPaletteCellSize).translated(viewItemRect.topLeft());
}

QPainterPath colorPaletteCheckMarkPath(const QRect& paletteCellRect)
{
    QPainterPath checkmarkPath;
    nx::style::RectCoordinates rectCoordinates(paletteCellRect);

    checkmarkPath.moveTo(rectCoordinates.x(0.2), rectCoordinates.y(0.5));
    checkmarkPath.lineTo(rectCoordinates.x(0.4), rectCoordinates.y(0.7));
    checkmarkPath.lineTo(rectCoordinates.x(0.8), rectCoordinates.y(0.3));

    return checkmarkPath;
}

QPen colorPaletteCheckMarkPen()
{
    static const auto kCheckMarkColor = nx::vms::client::core::colorTheme()->color(
        "camera.hotspots.paletteCheckMark");

    QPen checkMarkPen(kCheckMarkColor);
    checkMarkPen.setWidthF(2);
    checkMarkPen.setJoinStyle(Qt::MiterJoin);
    checkMarkPen.setCapStyle(Qt::FlatCap);

    return checkMarkPen;
}

void setupStyleOptionColors(
    QStyleOptionViewItem* itemViewOption,
    const QColor& color,
    const QColor& selectedColor)
{
    using namespace nx::vms::client::core;

    if (!itemViewOption->icon.isNull())
    {
        const auto normalIconPixmap =
            qnSkin->maximumSizePixmap(itemViewOption->icon, QIcon::Normal);

        QIcon colorizedIcon;
        colorizedIcon.addPixmap(qnSkin->colorize(normalIconPixmap, color), QIcon::Normal);
        colorizedIcon.addPixmap(qnSkin->colorize(normalIconPixmap, selectedColor), QIcon::Selected);

        itemViewOption->icon = colorizedIcon;
    }

    const auto itemIsEnabled = itemViewOption->state.testFlag(QStyle::State_Enabled);

    itemViewOption->palette.setColor(QPalette::Text, itemIsEnabled
        ? color
        : ColorTheme::transparent(color, nx::style::Hints::kDisabledItemOpacity));

    itemViewOption->palette.setColor(QPalette::HighlightedText, itemIsEnabled
        ? selectedColor
        : ColorTheme::transparent(selectedColor, nx::style::Hints::kDisabledItemOpacity));
}

} // namespace

namespace nx::vms::client::desktop {

CameraHotspotsItemDelegate::CameraHotspotsItemDelegate(QObject* parent):
    base_type(parent)
{
}

CameraHotspotsItemDelegate::~CameraHotspotsItemDelegate()
{
}

void CameraHotspotsItemDelegate::setItemViewHoverTracker(ItemViewHoverTracker* hoverTracker)
{
    m_hoverTracker = hoverTracker;
}

QList<QColor> CameraHotspotsItemDelegate::hotspotsPalette()
{
    static auto kPaletteColors = core::colorTheme()->colors("camera.hotspots.palette");
    return kPaletteColors;
}

std::optional<int> CameraHotspotsItemDelegate::paletteColorIndex(const QColor& color)
{
    for (int i = 0; i < hotspotsPalette().size(); ++i)
    {
        if (hotspotsPalette().at(i) == color)
            return i;
    }

    return std::nullopt;
}

std::optional<int> CameraHotspotsItemDelegate::paletteColorIndexAtPos(
    const QPoint& pos,
    const QRect& viewItemRect)
{
    for (int i = 0; i < hotspotsPalette().size(); ++i)
    {
        if (colorPaletteCellRect(i, viewItemRect).contains(pos))
            return i;
    }

    return std::nullopt;
}

QRect CameraHotspotsItemDelegate::itemContentsRect(
    const QModelIndex& index,
    QAbstractItemView* itemView) const
{
    if (!itemView || !index.isValid())
        return {};

    QStyleOptionViewItem option;
    option.widget = itemView;
    option.rect = itemView->visualRect(index);
    option.decorationSize =
        QSize(nx::style::Metrics::kDefaultIconSize, nx::style::Metrics::kDefaultIconSize);

    base_type::initStyleOption(&option, index);

    const auto style = option.widget->style();

    const auto itemTextRect =
        style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);

    const auto itemIconRect =
        style->subElementRect(QStyle::SE_ItemViewItemDecoration, &option, option.widget);

    return itemTextRect.united(itemIconRect).adjusted(1, 1, 1, 1);
}

void CameraHotspotsItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    base_type::paint(painter, styleOption, index);

    if (index.column() == CameraHotspotsItemModel::ColorPaletteColumn)
        paintColorPicker(painter, styleOption, index);
}

QSize CameraHotspotsItemDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    QSize result = base_type::sizeHint(option, index);

    if (index.column() == CameraHotspotsItemModel::ColorPaletteColumn)
    {
        result.setWidth(nx::style::Metrics::kStandardPadding
            + hotspotsPalette().size() * kPaletteCellSize.width()
            + (hotspotsPalette().size() - 1) * kPaletteCellSpacing
            + nx::style::Metrics::kDefaultTopLevelMargin);
    }
    else if (index.column() == CameraHotspotsItemModel::DeleteButtonColumn)
    {
        result.setWidth(result.width() + nx::style::Metrics::kDefaultTopLevelMargin);
    }

    return result;
}

void CameraHotspotsItemDelegate::paintColorPicker(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
    QnScopedPainterBrushRollback brushRollback(painter);
    QnScopedPainterOpacityRollback opacityRollback(painter);

    if (!styleOption.state.testFlag(QStyle::State_Enabled))
        painter->setOpacity(nx::style::Hints::kDisabledItemOpacity);

    for (int colorIndex = 0; colorIndex < hotspotsPalette().size(); ++colorIndex)
    {
        painter->setBrush(hotspotsPalette().at(colorIndex));
        painter->drawRoundedRect(colorPaletteCellRect(colorIndex, styleOption.rect),
            kPaletteCellCornerRadius, kPaletteCellCornerRadius);
    }

    painter->setBrush(Qt::NoBrush);

    const auto hotspotAccentColor =
        index.data(CameraHotspotsItemModel::HotspotColorRole).value<QColor>();

    if (const auto hotspotAccentColorIndex = paletteColorIndex(hotspotAccentColor))
    {
        painter->setPen(colorPaletteCheckMarkPen());
        painter->drawPath(colorPaletteCheckMarkPath(
            colorPaletteCellRect(*hotspotAccentColorIndex, styleOption.rect)));
    }
}

void CameraHotspotsItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    // Keyboard focus decoration is never displayed across client item views.
    option->state.setFlag(QStyle::State_HasFocus, false);

    if (index.column() != CameraHotspotsItemModel::DeleteButtonColumn)
    {
        option->font.setWeight(QFont::Medium);
        option->fontMetrics = QFontMetrics(option->font);
    }

    if (index.column() == CameraHotspotsItemModel::TargetColumn)
    {
        const auto cameraIdData = index.data(CameraHotspotsItemModel::HotspotCameraIdRole);
        const auto cameraResourceData = index.data(Qn::ResourceRole);

        if (!cameraIdData.isNull() && cameraResourceData.isNull()) //< Camera no more exists.
        {
            static const auto kInvalidCameraColor = core::colorTheme()->color("red_core");
            static const auto kSelectedInvalidCameraColor = core::colorTheme()->color("red_l2");
            setupStyleOptionColors(option, kInvalidCameraColor, kSelectedInvalidCameraColor);
        }

        if (m_hoverTracker && m_hoverTracker->hoveredIndex() == index)
        {
            option->palette.setColor(
                QPalette::Text,
                option->palette.color(QPalette::HighlightedText));
            option->icon = option->icon.pixmap(option->rect.size(), QIcon::Selected);
        }

        if (!option->state.testFlag(QStyle::State_Enabled))
        {
            // Camera icon in QIcon::Disabled mode looks wrong, so make icons in the disabled item
            // view appear like anywhere in the app, as icon in QIcon::Normal mode with opacity
            // defined by nx::style::Hints::kDisabledItemOpacity constant.
            auto disabledIconPixmap = option->icon.pixmap(option->rect.size(), QIcon::Normal);
            QPainter disabledPixmapPainter(&disabledIconPixmap);
            disabledPixmapPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            disabledPixmapPainter.fillRect(disabledIconPixmap.rect(),
                QColor::fromRgbF(0, 0, 0, nx::style::Hints::kDisabledItemOpacity));
            option->icon.addPixmap(disabledIconPixmap, QIcon::Disabled);
        }
    }

    if (index.column() == CameraHotspotsItemModel::PointedCheckBoxColumn)
    {
        const auto checkStateData = index.data(Qt::CheckStateRole);
        const bool isChecked = !checkStateData.isNull()
            && checkStateData.value<Qt::CheckState>() != Qt::Unchecked;

        if (option->state.testFlag(QStyle::State_Selected) || isChecked)
        {
            option->palette.setColor(
                QPalette::Text,
                option->palette.color(QPalette::HighlightedText));
        }
    }

    if (index.column() == CameraHotspotsItemModel::DeleteButtonColumn)
    {
        auto color = option->palette.color(QPalette::WindowText);
        auto selectedColor = option->palette.color(QPalette::Text);

        auto makeColorsLigther =
            [&color, &selectedColor]
            {
                color = core::colorTheme()->lighter(color, 2);
                selectedColor = core::colorTheme()->lighter(selectedColor, 2);
            };

        if (option->state.testFlag(QStyle::State_MouseOver))
            makeColorsLigther(); //< Whole row is hovered.

        if (m_hoverTracker && m_hoverTracker->hoveredIndex() == index)
            makeColorsLigther(); //< In addition, 'Delete' button-alike item is hovered.

        setupStyleOptionColors(option, color, selectedColor);
    }
}

} // namespace nx::vms::client::desktop
