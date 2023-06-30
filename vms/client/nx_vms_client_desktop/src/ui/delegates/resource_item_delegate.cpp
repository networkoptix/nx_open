// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_item_delegate.h"

#include <QtCore/QtMath>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>

#include <client/client_meta_types.h>
#include <common/common_globals.h>
#include <common/common_module.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_resource.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource_views/data/resource_extra_status.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/virtual_camera/virtual_camera_manager.h>
#include <nx/vms/client/desktop/virtual_camera/virtual_camera_state.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/scoped_painter_rollback.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

namespace {

constexpr int kSeparatorItemHeight = 16;
constexpr int kExtraTextMargin = 5;
static constexpr int kMaxResourceNameLength = 120; //< TODO: #sivanov Move to common place to use.

bool isCollapsibleNode(const QModelIndex& index)
{
    NX_ASSERT(index.model());
    if (!index.model())
        return false;

    return index.model()->rowCount(index) > 0;
}

} // namespace

QnResourceItemDelegate::QnResourceItemDelegate(QObject* parent):
    base_type(parent),
    m_recordingIcon(qnSkin->icon("tree/record_on.svg")),
    m_scheduledIcon(qnSkin->icon("tree/record_part.svg")),
    m_lockedIcon(qnSkin->icon("tree/locked.svg")),
    m_fixedHeight(nx::style::Metrics::kViewRowHeight),
    m_rowSpacing(0),
    m_customInfoLevel(Qn::ResourceInfoLevel::RI_Invalid),
    m_options(NoOptions)
{
}

Workbench* QnResourceItemDelegate::workbench() const
{
    return m_workbench.data();
}

void QnResourceItemDelegate::setWorkbench(Workbench* workbench)
{
    m_workbench = workbench;
}

int QnResourceItemDelegate::rowSpacing() const
{
    return m_rowSpacing;
}

void QnResourceItemDelegate::setRowSpacing(int value)
{
    m_rowSpacing = qMax(value, 0);
}

Qn::ResourceInfoLevel QnResourceItemDelegate::customInfoLevel() const
{
    return m_customInfoLevel;
}

void QnResourceItemDelegate::setCustomInfoLevel(Qn::ResourceInfoLevel value)
{
    m_customInfoLevel = value;
}

QnResourceItemDelegate::Options QnResourceItemDelegate::options() const
{
    return m_options;
}

void QnResourceItemDelegate::setOptions(Options value)
{
    m_options = value;
}

int QnResourceItemDelegate::fixedHeight() const
{
    return m_fixedHeight;
}

void QnResourceItemDelegate::setFixedHeight(int value)
{
    m_fixedHeight = qMax(value, 0);
}

void QnResourceItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& styleOption, const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    QStyle* style = option.widget ? option.widget->style() : QApplication::style();

    /* If item is separator, draw it: */
    if (option.features == QStyleOptionViewItem::None)
    {
        int y = option.rect.top() + option.rect.height() / 2;
        QnScopedPainterPenRollback penRollback(painter, option.palette.color(QPalette::Midlight));
        painter->drawLine(0, y, option.rect.right(), y);
        return;
    }

    /* Select icon and text color by item state: */
    QColor mainColor, extraColor;
    QIcon::Mode iconMode(QIcon::Normal);

    switch (itemState(index))
    {
        case ItemState::normal:
            iconMode = QIcon::Normal;
            mainColor = core::colorTheme()->color("resourceTree.mainText");
            extraColor = core::colorTheme()->color("resourceTree.extraText");
            break;

        case ItemState::selected:
            iconMode = QIcon::Selected;
            mainColor = core::colorTheme()->color("resourceTree.mainTextSelected");
            extraColor = core::colorTheme()->color("resourceTree.extraTextSelected");
            break;

        case ItemState::accented:
            iconMode = QIcon::Active;
            mainColor = core::colorTheme()->color("resourceTree.mainTextAccented");
            extraColor = core::colorTheme()->color("resourceTree.extraTextAccented");
            break;

        default:
            NX_ASSERT(false); //< Should never get here.
    }

    if (!option.state.testFlag(QStyle::State_Selected))
    {
        if (option.state.testFlag(State_Error))
        {
            // Use error text color.
            mainColor = core::colorTheme()->color("red_l2");
            iconMode = QnIcon::Error;
        }
        else
        {
            // Try to consider model's foreground color override.
            const auto modelColorOverride = index.data(Qt::ForegroundRole);
            if (modelColorOverride.canConvert<QBrush>())
                mainColor = modelColorOverride.value<QBrush>().color();
        }
    }

    // TODO #vkutin Get rid of this and draw checkboxes in this delegate like everything else
    /* Check indicators in this implementation are handled elsewhere: */
    if (option.features.testFlag(QStyleOptionViewItem::HasCheckIndicator))
    {
        mainColor.setAlphaF(option.palette.color(QPalette::Text).alphaF());
        option.palette.setColor(QPalette::Text, mainColor);
        style->drawControl(QStyle::CE_ItemViewItem, &option, painter, option.widget);
        return;
    }

    /* Due to Qt bug, State_Editing is not set in option.state, so detect editing differently: */
    const QAbstractItemView* view = qobject_cast<const QAbstractItemView*>(option.widget);
    const bool editing = view && view->indexWidget(option.index);

    /* Obtain sub-element rectangles: */
    const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
    const QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &option, option.widget);

    /* Paint background: */
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    /* Set opacity if disabled: */
    QnScopedPainterOpacityRollback opacityRollback(painter);
    if (!option.state.testFlag(QStyle::State_Enabled))
        painter->setOpacity(painter->opacity() * nx::style::Hints::kDisabledItemOpacity);

    /* Draw icon: */
    if (option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        option.icon.paint(painter, iconRect, option.decorationAlignment, iconMode, QIcon::On);

    /* Draw text: */
    if (!editing)
    {
        QString baseName;
        QString extraInfo;
        getDisplayInfo(index, baseName, extraInfo);

        const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; /* As in Qt */
        const int textEnd = textRect.right() - textPadding + 1;

        QPoint textPos = textRect.topLeft() + QPoint(textPadding, option.fontMetrics.ascent()
            + qCeil((textRect.height() - option.fontMetrics.height()) / 2.0));

        if (textEnd > textPos.x())
        {
            const auto devicePixelRatio = painter->device()->devicePixelRatio();

            const auto main = m_textPixmapCache.pixmap(baseName, option.font, mainColor,
                devicePixelRatio, textEnd - textPos.x() + 1, option.textElideMode);

            if (!main.pixmap.isNull())
            {
                painter->drawPixmap(textPos + main.origin, main.pixmap);
                textPos.rx() += main.origin.x() + main.size().width() + kExtraTextMargin;
            }

            if (textEnd > textPos.x() && !main.elided() && !extraInfo.isEmpty())
            {
                option.font.setWeight(QFont::Normal);

                const auto extra = m_textPixmapCache.pixmap(extraInfo, option.font, extraColor,
                    devicePixelRatio, textEnd - textPos.x(), option.textElideMode);

                if (!extra.pixmap.isNull())
                    painter->drawPixmap(textPos + extra.origin, extra.pixmap);
            }
        }
    }

    paintExtraStatus(painter, iconRect, index);
}

QSize QnResourceItemDelegate::sizeHint(const QStyleOptionViewItem& styleOption, const QModelIndex& index) const
{
    /* Initialize style option: */
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    /* If item is separator, return separator size: */
    if (option.features == QStyleOptionViewItem::None)
        return QSize(0, kSeparatorItemHeight + m_rowSpacing);

    // TODO #vkutin Keep this while checkboxed items are painted by default implementation:
    if (option.features.testFlag(QStyleOptionViewItem::HasCheckIndicator))
        return base_type::sizeHint(option, index);

    QStyle* style = option.widget ? option.widget->style() : QApplication::style();

    /* Let the style calculate text rect so we can determine side paddings dictated by it: */
    option.rect.setSize(QSize(10000, 20)); // some really big width and any nonzero height
    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);

    int paddingsFromStyle = textRect.left() + (option.rect.right() - textRect.right());

    /* Initial size: */
    int width = paddingsFromStyle;
    int height = option.decorationSize.height();

    /* Adjust size to text: */
    {
        QString baseName;
        QString extraInfo;
        getDisplayInfo(index, baseName, extraInfo);

        int leftRightPadding = (style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget) + 1) * 2; // As in Qt

        /* Adjust height to text: */
        height = qMax(height, option.fontMetrics.height());

        /* Width of the main text: */
        width += option.fontMetrics.size({}, baseName).width();

        if (!extraInfo.isEmpty())
        {
            /* Width of the extra text: */
            option.font.setWeight(QFont::Normal);
            QFontMetrics metrics(option.font);
            width += option.fontMetrics.size({}, extraInfo).width() + leftRightPadding;
        }

        /* Add paddings: */
        width += leftRightPadding;
    }

    /* If fixed height is requested, override calculated height: */
    if (m_fixedHeight > 0)
        height = m_fixedHeight;

    /* Add extra row spacing and return results: */
    return QSize(width, height + m_rowSpacing);
}

void QnResourceItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    /* Init font options: */
    option->font.setWeight(QFont::Medium);
    option->fontMetrics = QFontMetrics(option->font);

    /* Save default decoration size: */
    QSize defaultDecorationSize = option->decorationSize;

    /* If icon size is explicitly specified in itemview, decorationSize already holds that value.
     * If icon size is not specified in itemview, set decorationSize to something big.
     * It will be treated as maximal allowed size: */
    auto view = qobject_cast<const QAbstractItemView*>(option->widget);
    if ((!view || !view->iconSize().isValid()) && m_fixedHeight > 0)
        option->decorationSize.setHeight(qMin(defaultDecorationSize.height(), m_fixedHeight));

    /* Call inherited implementation.
     * When it configures item icon, it sets decorationSize to actual icon size: */
    base_type::initStyleOption(option, index);

    /* But if the item has no icon, restore decoration size to saved default: */
    if (option->icon.isNull())
        option->decorationSize = defaultDecorationSize;

    const auto nodeType = index.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
    if (isSeparatorNode(nodeType))
        option->features = QStyleOptionViewItem::None;
    else
        option->features |= QStyleOptionViewItem::HasDisplay;

    if (!option->state.testFlag(QStyle::State_Enabled))
        option->state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);

    // Determine validity.
    if (m_options.testFlag(ValidateOnlyChecked))
    {
        const auto checkState = rowCheckState(index);
        if (checkState.canConvert<int>() && checkState.toInt() == Qt::Unchecked)
            return;
    }

    const auto isValid = index.data(Qn::ValidRole);
    if (isValid.canConvert<bool>() && !isValid.toBool())
        option->state |= State_Error;

    const auto validationState = index.data(Qn::ValidationStateRole);
    if (validationState.canConvert<QValidator::State>()
        && validationState.value<QValidator::State>() == QValidator::Invalid)
    {
        option->state |= State_Error;
    }
}

QWidget* QnResourceItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    /* Create editor: */
    QWidget* editor = base_type::createEditor(parent, option, index);

    if (auto lineEdit = qobject_cast<QLineEdit*>(editor))
        lineEdit->setMaxLength(kMaxResourceNameLength);

    /* Change editor font: */
    QFont font(editor->font());
    font.setWeight(QFont::Medium);
    editor->setFont(font);

    /* Set text and selected text editor colors by item state: */
    QPalette editorPalette = editor->palette();
    switch (itemState(index))
    {
        case ItemState::normal:
            editorPalette.setColor(QPalette::Text,
                core::colorTheme()->color("resourceTree.mainText"));
            editorPalette.setColor(QPalette::HighlightedText,
                core::colorTheme()->color("resourceTree.mainText"));
            break;

        case ItemState::selected:
            editorPalette.setColor(QPalette::Text,
                core::colorTheme()->color("resourceTree.mainTextSelected"));
            editorPalette.setColor(QPalette::HighlightedText,
                core::colorTheme()->color("resourceTree.mainTextSelected"));
            break;

        case ItemState::accented:
            editorPalette.setColor(QPalette::Text,
                core::colorTheme()->color("resourceTree.mainTextAccented"));
            editorPalette.setColor(QPalette::HighlightedText,
                core::colorTheme()->color("resourceTree.mainTextAccented"));
            break;

        default:
            NX_ASSERT(false); // Should never get here
    }

    editor->setPalette(editorPalette);

    return editor;
}

void QnResourceItemDelegate::destroyEditor(QWidget* editor, const QModelIndex& index) const
{
    // TODO: #QTBUG If focus is not cleared, we get crashes. Should be fixed in QWidget.
    editor->clearFocus();
    base_type::destroyEditor(editor, index);
}

QnResourceItemDelegate::ItemState QnResourceItemDelegate::itemState(const QModelIndex& index) const
{
    if (m_options.testFlag(HighlightChecked))
    {
        const auto checkState = rowCheckState(index);
        if (checkState.canConvert<int>())
            return checkState.toInt() == Qt::Checked ? ItemState::selected : ItemState::normal;
    }

    if (!workbench())
        return ItemState::normal;

    const auto nodeType = index.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();

    switch (nodeType)
    {
        case ResourceTree::NodeType::currentSystem:
        case ResourceTree::NodeType::currentUser:
            return ItemState::selected;

        /*
         * Media resources are Selected when they are placed on the scene, Active - when chosen
         * as current central item.
         * Layouts are Selected when they are opened on the scene.
         * Videowall items are Selected if we are in videowall review mode, Active - when chosen
         * as current central item.
         * Videowall is Selected when we are in videowall review or control mode.
        */

        case ResourceTree::NodeType::resource:
        case ResourceTree::NodeType::sharedLayout:
        case ResourceTree::NodeType::edge:
        case ResourceTree::NodeType::sharedResource:
        {
            QnResourcePtr resource = index.data(core::ResourceRole).value<QnResourcePtr>();
            NX_ASSERT(resource);
            if (!resource)
                return ItemState::normal;

            if (resource->hasFlags(Qn::videowall))
                return itemStateForVideoWall(index);

            if (resource->hasFlags(Qn::layout))
                return itemStateForLayout(index);

            return itemStateForMediaResource(index);
        }

        case ResourceTree::NodeType::recorder:
            return itemStateForRecorder(index);

        case ResourceTree::NodeType::layoutItem:
            return itemStateForLayoutItem(index);

        case ResourceTree::NodeType::videoWallItem:
            return itemStateForVideoWallItem(index);

        case ResourceTree::NodeType::showreel:
            return itemStateForShowreel(index);

        default:
            break;
    }
    return ItemState::normal;
}

QnResourceItemDelegate::ItemState QnResourceItemDelegate::itemStateForMediaResource(
    const QModelIndex& index) const
{
    /*
     * Media resources are Selected when they are placed on the scene, Active - when chosen as
     * current central item.
     */
    const auto currentLayout = workbench()->currentLayout()->resource();
    if (!currentLayout)
        return ItemState::normal;

    const auto centralItem = workbench()->item(Qn::CentralRole);
    QnResourcePtr resource = index.data(core::ResourceRole).value<QnResourcePtr>();

    if (centralItem && centralItem->resource() == resource)
        return ItemState::accented;

    if (resourceBelongsToLayout(resource, currentLayout))
        return ItemState::selected;

    return ItemState::normal;
}

QnResourceItemDelegate::ItemState QnResourceItemDelegate::itemStateForLayout(
    const QModelIndex& index) const
{
    /* Layouts are Selected when they are opened on the scene. */

    const auto currentLayout = workbench()->currentLayout()->resource();
    QnResourcePtr resource = index.data(core::ResourceRole).value<QnResourcePtr>();

    if (currentLayout && currentLayout == resource)
        return ItemState::selected;
    return ItemState::normal;
}

QnResourceItemDelegate::ItemState QnResourceItemDelegate::itemStateForRecorder(
    const QModelIndex& index) const
{
    /* Recorders have the max state of all its children. */

    const auto model = index.model();
    NX_ASSERT(model);
    if (!model)
        return ItemState::normal;

    bool hasSelectedChild = false;
    for (int i = 0; i < model->rowCount(index); ++i)
    {
        const auto child = index.model()->index(i, 0, index);
        const auto childState = itemStateForMediaResource(child);
        if (childState == ItemState::accented)
            return ItemState::accented;

        hasSelectedChild |= (childState == ItemState::selected);
    }

    return hasSelectedChild
        ? ItemState::selected
        : ItemState::normal;
}

QnResourceItemDelegate::ItemState QnResourceItemDelegate::itemStateForLayoutItem(
    const QModelIndex& index) const
{
    /*
    * Layout items are Selected when layout is placed on the scene, Active - when chosen as current
    * central item.
    */
    const auto currentLayout = workbench()->currentLayout()->resource();
    if (!currentLayout)
        return ItemState::normal;

    const auto owningLayout = index.parent().data(core::ResourceRole).value<QnResourcePtr>();
    if (!owningLayout || owningLayout != currentLayout)
        return ItemState::normal;

    const auto uuid = index.data(Qn::ItemUuidRole).value<QnUuid>();
    const auto centralItem = workbench()->item(Qn::CentralRole);

    if (centralItem && centralItem->uuid() == uuid)
        return ItemState::accented;

    return ItemState::selected;
}

QnResourceItemDelegate::ItemState QnResourceItemDelegate::itemStateForVideoWall(
    const QModelIndex& index) const
{
    /* Videowall is Selected when we are in videowall review or control mode. */

    QnResourcePtr resource = index.data(core::ResourceRole).value<QnResourcePtr>();
    const auto videowall = resource.dynamicCast<QnVideoWallResource>();
    NX_ASSERT(videowall);
    if (!videowall)
        return ItemState::normal;

    auto layout = workbench()->currentLayout();
    auto videoWallControlModeUuid = layout->data(Qn::VideoWallItemGuidRole).value<QnUuid>();
    if (!videoWallControlModeUuid.isNull())
    {
        return videowall->items()->hasItem(videoWallControlModeUuid)
            ? ItemState::selected
            : ItemState::normal;
    }

    auto videoWallReview = layout->data(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    return (videoWallReview == videowall)
        ? ItemState::selected
        : ItemState::normal;
}

QnResourceItemDelegate::ItemState QnResourceItemDelegate::itemStateForVideoWallItem(
    const QModelIndex& index) const
{
    /*
     * Videowall items are Selected if we are in videowall review mode AND item belongs to current
     * videowall. Active - when chosen as current central item OR in videowall review mode.
     */
    const auto owningVideoWall = index.parent().data(core::ResourceRole).value<QnResourcePtr>()
        .dynamicCast<QnVideoWallResource>();
    NX_ASSERT(owningVideoWall);
    if (!owningVideoWall)
        return ItemState::normal;

    QnUuid uuid = index.data(Qn::ItemUuidRole).value<QnUuid>();
    NX_ASSERT(!uuid.isNull());

    auto layout = workbench()->currentLayout();
    auto videoWallControlModeUuid = layout->data(Qn::VideoWallItemGuidRole).value<QnUuid>();
    if (!videoWallControlModeUuid.isNull())
    {
        return videoWallControlModeUuid == uuid
            ? ItemState::accented
            : ItemState::normal;
    }

    auto videoWall = layout->data(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    if (videoWall != owningVideoWall)
        return ItemState::normal;

    auto centralItem = workbench()->item(Qn::CentralRole);
    if (!centralItem || uuid.isNull())
        return ItemState::selected;

    auto layoutResource = layout->resource();
    auto indices = layoutResource->itemData(
        centralItem->uuid(),
        Qn::VideoWallItemIndicesRole).value<QnVideoWallItemIndexList>();

    for (const auto& itemIndex: indices)
    {
        NX_ASSERT(itemIndex.videowall() == videoWall);
        if (itemIndex.uuid() == uuid)
            return ItemState::accented;
    }
    return ItemState::selected;
}

QnResourceItemDelegate::ItemState QnResourceItemDelegate::itemStateForShowreel(
    const QModelIndex& index) const
{
    /* Showreels are Selected when they are opened on the scene. */
    const QnUuid currentShowreelId = workbench()->currentLayout()->data(Qn::ShowreelUuidRole)
        .value<QnUuid>();
    if (!currentShowreelId.isNull()
        && index.data(core::UuidRole).value<QnUuid>() == currentShowreelId)
    {
        return ItemState::selected;
    }
    return ItemState::normal;
}

void QnResourceItemDelegate::getDisplayInfo(const QModelIndex& index, QString& baseName, QString& extInfo) const
{
    baseName = index.data(Qt::DisplayRole).toString();
    extInfo = QString();

    /* Two-component text from resource information: */
    auto infoLevel = m_customInfoLevel;
    if (infoLevel == Qn::RI_Invalid)
        infoLevel = appContext()->localSettings()->resourceInfoLevel();

    QnResourcePtr resource = index.data(core::ResourceRole).value<QnResourcePtr>();
    if (resource && resource->hasFlags(Qn::virtual_camera))
        infoLevel = Qn::RI_FullInfo;

    if (infoLevel == Qn::RI_NameOnly)
        return;

    const auto nodeType = index.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();

    if (nodeType == ResourceTree::NodeType::videoWallItem)
    {
        // skip videowall screens
    }
    else if (nodeType == ResourceTree::NodeType::recorder)
    {
        auto firstChild = index.model()->index(0, 0, index);
        if (!firstChild.isValid()) /* This can happen in rows deleting */
            return;
        QnResourcePtr resource = firstChild.data(core::ResourceRole).value<QnResourcePtr>();
        extInfo = QnResourceDisplayInfo(resource).extraInfo();
    }
    else
    {
        if (!resource)
            return;

        if ((nodeType == ResourceTree::NodeType::layoutItem
                || nodeType == ResourceTree::NodeType::sharedResource)
            && resource->hasFlags(Qn::server))
        {
            extInfo = QString("%1 %2").arg(nx::UnicodeChars::kEnDash, tr("Health Monitor"));
        }
        else
        {
            QnResourceDisplayInfo info(resource);
            extInfo = info.extraInfo();
        }

        if (resource->hasFlags(Qn::user) && !extInfo.isEmpty())
            extInfo = QString("%1 %2").arg(nx::UnicodeChars::kEnDash, extInfo);

        if (resource->hasFlags(Qn::virtual_camera))
        {
            QnSecurityCamResourcePtr camera = resource.dynamicCast<QnSecurityCamResource>();
            auto systemContext = SystemContext::fromResource(camera);

            VirtualCameraState state = systemContext->virtualCameraManager()->state(camera);
            if (state.isRunning())
                extInfo = QString("%1 %2").arg(
                    "%1 %2",
                    nx::UnicodeChars::kEnDash,
                    QString::number(state.progress()) + lit("%"));
        }
    }
}

int QnResourceItemDelegate::checkBoxColumn() const
{
    return m_checkBoxColumn;
}

void QnResourceItemDelegate::setCheckBoxColumn(int value)
{
    m_checkBoxColumn = value;
}

QVariant QnResourceItemDelegate::rowCheckState(const QModelIndex& index) const
{
    if (m_checkBoxColumn < 0)
        return QVariant();

    const auto checkIndex = index.sibling(index.row(), m_checkBoxColumn);
    return checkIndex.data(Qt::CheckStateRole);
}

void QnResourceItemDelegate::paintExtraStatus(
    QPainter* painter,
    const QRect& iconRect,
    const QModelIndex& index) const
{
    const auto extraStatus = index.data(Qn::ResourceExtraStatusRole).value<ResourceExtraStatus>();
    if (extraStatus == ResourceExtraStatus())
        return;

    QRect extraIconRect(iconRect);

    // Check if there are too much icons for this indentaion level.
    const auto shiftIconLeft =
        [&extraIconRect]
        {
            const int pos = extraIconRect.left() - nx::style::Metrics::kDefaultIconSize;
            extraIconRect.moveLeft(pos);
            return pos >= 0;
        };

    // Leave space for collapser if needed.
    if (isCollapsibleNode(index))
        shiftIconLeft();

    // Draw "recording" or "scheduled" icon.

    // Draw recording status icon.
    if (m_options.testFlag(RecordingIcons))
    {
        const auto recordingIcon = QnResourceIconCache::cameraRecordingStatusIcon(extraStatus);
        if (!recordingIcon.isNull())
        {
            if (!shiftIconLeft())
                return;

            recordingIcon.paint(painter, extraIconRect);
        }
    }

    // Draw "locked" icon.
    if (m_options.testFlag(LockedIcons) && extraStatus.testFlag(ResourceExtraStatusFlag::locked))
    {
        if (!shiftIconLeft())
            return;

        m_lockedIcon.paint(painter, extraIconRect);
    }
}
