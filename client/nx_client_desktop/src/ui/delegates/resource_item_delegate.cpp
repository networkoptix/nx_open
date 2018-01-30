#include "resource_item_delegate.h"

#include <QtCore/QtMath>

#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>

#include <common/common_globals.h>
#include <common/common_module.h>

#include <core/resource/camera_history.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item_index.h>

#include <core/resource_management/resource_runtime_data.h>

#include <client/client_meta_types.h>
#include <client/client_settings.h>
#include <client/client_module.h>

#include <ui/style/skin.h>
#include <ui/style/globals.h>
#include <ui/style/helper.h>
#include <ui/style/noptix_style.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>

#include <nx/client/desktop/utils/wearable_manager.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {

constexpr int kSeparatorItemHeight = 16;
constexpr int kExtraTextMargin = 5;
static constexpr int kMaxResourceNameLength = 120; // TODO: #GDM Think about common place to use.

} // namespace


QnResourceItemDelegate::QnResourceItemDelegate(QObject* parent):
    base_type(parent),
    m_workbench(),
    m_recordingIcon(qnSkin->icon("tree/recording.png")),
    m_scheduledIcon(qnSkin->icon("tree/scheduled.png")),
    m_buggyIcon(qnSkin->icon("tree/buggy.png")),
    m_colors(),
    m_fixedHeight(style::Metrics::kViewRowHeight),
    m_rowSpacing(0),
    m_customInfoLevel(Qn::ResourceInfoLevel::RI_Invalid),
    m_options(NoOptions)
{
}

QnWorkbench* QnResourceItemDelegate::workbench() const
{
    return m_workbench.data();
}

void QnResourceItemDelegate::setWorkbench(QnWorkbench* workbench)
{
    m_workbench = workbench;
}

const QnResourceItemColors& QnResourceItemDelegate::colors() const
{
    return m_colors;
}

void QnResourceItemDelegate::setColors(const QnResourceItemColors& colors)
{
    m_colors = colors;
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

    if (option.widget && option.widget->rect().bottom() < option.rect.bottom() &&
        option.widget->property(Qn::HideLastRowInTreeIfNotEnoughSpace).toBool())
    {
        return;
    }

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
    QIcon::Mode iconMode;

    switch (itemState(index))
    {
        case ItemState::normal:
            iconMode = QIcon::Normal;
            mainColor = m_colors.mainText;
            extraColor = m_colors.extraText;
            break;

        case ItemState::selected:
            iconMode = QIcon::Selected;
            mainColor = m_colors.mainTextSelected;
            extraColor = m_colors.extraTextSelected;
            break;

        case ItemState::accented:
            iconMode = QIcon::Active;
            mainColor = m_colors.mainTextAccented;
            extraColor = m_colors.extraTextAccented;
            break;

        default:
            NX_ASSERT(false); //< Should never get here.
    }

    if (!option.state.testFlag(QStyle::State_Selected))
    {
        if (option.state.testFlag(State_Error))
        {
            // Use error text color.
            mainColor = qnGlobals->errorTextColor();
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
        painter->setOpacity(painter->opacity() * style::Hints::kDisabledItemOpacity);

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
            const auto main = m_textPixmapCache.pixmap(baseName, option.font, mainColor,
                textEnd - textPos.x(), option.textElideMode);

            if (!main.pixmap.isNull())
            {
                painter->drawPixmap(textPos + main.origin, main.pixmap);
                textPos.rx() += main.origin.x() + main.size().width() + kExtraTextMargin;
            }

            if (textEnd > textPos.x() && !main.elided() && !extraInfo.isEmpty())
            {
                option.font.setWeight(QFont::Normal);

                const auto extra = m_textPixmapCache.pixmap(extraInfo, option.font, extraColor,
                    textEnd - textPos.x(), option.textElideMode);

                if (!extra.pixmap.isNull())
                    painter->drawPixmap(textPos + extra.origin, extra.pixmap);
            }
        }
    }

    QRect extraIconRect(iconRect);
    const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();

    /* Draw "recording" or "scheduled" icon: */
    if (m_options.testFlag(RecordingIcons))
    {
        extraIconRect.moveLeft(extraIconRect.left() - extraIconRect.width());

        const bool recording = camera && camera->getStatus() == Qn::Recording;
        const bool scheduled = camera && !camera->isScheduleDisabled();

        if (recording || scheduled)
            (recording ? m_recordingIcon : m_scheduledIcon).paint(painter, extraIconRect);
    }

    /* Draw "problems" icon: */
    if (m_options.testFlag(ProblemIcons))
    {
        extraIconRect.moveLeft(extraIconRect.left() - extraIconRect.width());
        if (camera && camera->statusFlags().testFlag(Qn::CSF_HasIssuesFlag))
            m_buggyIcon.paint(painter, extraIconRect);
    }
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
        width += option.fontMetrics.width(baseName);

        if (!extraInfo.isEmpty())
        {
            /* Width of the extra text: */
            option.font.setWeight(QFont::Normal);
            QFontMetrics metrics(option.font);
            width += option.fontMetrics.width(extraInfo) + leftRightPadding;
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
    option->font.setWeight(QFont::DemiBold);
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

    Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();
    if (Qn::isSeparatorNode(nodeType))
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
    font.setWeight(QFont::DemiBold);
    editor->setFont(font);

    /* Set text and selected text editor colors by item state: */
    QPalette editorPalette = editor->palette();
    switch (itemState(index))
    {
        case ItemState::normal:
            editorPalette.setColor(QPalette::Text, m_colors.mainText);
            editorPalette.setColor(QPalette::HighlightedText, m_colors.mainText);
            break;

        case ItemState::selected:
            editorPalette.setColor(QPalette::Text, m_colors.mainTextSelected);
            editorPalette.setColor(QPalette::HighlightedText, m_colors.mainTextSelected);
            break;

        case ItemState::accented:
            editorPalette.setColor(QPalette::Text, m_colors.mainTextAccented);
            editorPalette.setColor(QPalette::HighlightedText, m_colors.mainTextAccented);
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

bool QnResourceItemDelegate::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::ContextMenu)
        return true; /* Ignore context menu events as they don't work well with editors embedded into graphics scene. */

    return base_type::eventFilter(object, event);
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

    Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();

    switch (nodeType)
    {
        case Qn::CurrentSystemNode:
        case Qn::CurrentUserNode:
            return ItemState::selected;

        /*
         * Media resources are Selected when they are placed on the scene, Active - when chosen
         * as current central item.
         * Layouts are Selected when they are opened on the scene.
         * Videowall items are Selected if we are in videowall review mode, Active - when chosen
         * as current central item.
         * Videowall is Selected when we are in videowall review or control mode.
        */

        case Qn::ResourceNode:
        case Qn::SharedLayoutNode:
        case Qn::EdgeNode:
        case Qn::SharedResourceNode:
        {
            QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
            NX_ASSERT(resource);
            if (!resource)
                return ItemState::normal;

            if (resource->hasFlags(Qn::videowall))
                return itemStateForVideoWall(index);

            if (resource->hasFlags(Qn::layout))
                return itemStateForLayout(index);

            return itemStateForMediaResource(index);
        }

        case Qn::RecorderNode:
            return itemStateForRecorder(index);

        case Qn::LayoutItemNode:
            return itemStateForLayoutItem(index);

        case Qn::VideoWallItemNode:
            return itemStateForVideoWallItem(index);

        case Qn::LayoutTourNode:
            return itemStateForLayoutTour(index);

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
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();

    if (centralItem && centralItem->resource() == resource)
        return ItemState::accented;

    if (currentLayout->layoutResourceIds().contains(resource->getId()))
        return ItemState::selected;

    return ItemState::normal;
}

QnResourceItemDelegate::ItemState QnResourceItemDelegate::itemStateForLayout(
    const QModelIndex& index) const
{
    /* Layouts are Selected when they are opened on the scene. */

    const auto currentLayout = workbench()->currentLayout()->resource();
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();

    if (currentLayout && currentLayout == resource)
        return ItemState::selected;
    return ItemState::normal;
}

QnResourceItemDelegate::ItemState QnResourceItemDelegate::itemStateForRecorder(
    const QModelIndex& index) const
{
    /* Recorders have the max state of all its children. */

    const auto model = index.model();
    NX_EXPECT(model);
    if (!model)
        return ItemState::normal;

    bool hasSelectedChild = false;
    for (int i = 0; i < model->rowCount(index); ++i)
    {
        const auto child = index.child(i, 0);
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

    const auto owningLayout = index.parent().data(Qn::ResourceRole).value<QnResourcePtr>();
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

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    const auto videowall = resource.dynamicCast<QnVideoWallResource>();
    NX_EXPECT(videowall);
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
    const auto owningVideoWall = index.parent().data(Qn::ResourceRole).value<QnResourcePtr>()
        .dynamicCast<QnVideoWallResource>();
    NX_EXPECT(owningVideoWall);
    if (!owningVideoWall)
        return ItemState::normal;

    QnUuid uuid = index.data(Qn::ItemUuidRole).value<QnUuid>();
    NX_EXPECT(!uuid.isNull());

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

    auto indices = qnResourceRuntimeDataManager->layoutItemData(centralItem->uuid(),
        Qn::VideoWallItemIndicesRole).value<QnVideoWallItemIndexList>();

    for (const auto& itemIndex: indices)
    {
        NX_EXPECT(itemIndex.videowall() == videoWall);
        if (itemIndex.uuid() == uuid)
            return ItemState::accented;
    }
    return ItemState::selected;
}

QnResourceItemDelegate::ItemState QnResourceItemDelegate::itemStateForLayoutTour(
    const QModelIndex& index) const
{
    /* Layout Tours are Selected when they are opened on the scene. */
    const QnUuid currentTourId = workbench()->currentLayout()->data(Qn::LayoutTourUuidRole).value<QnUuid>();
    if (!currentTourId.isNull() && index.data(Qn::UuidRole).value<QnUuid>() == currentTourId)
        return ItemState::selected;
    return ItemState::normal;
}

void QnResourceItemDelegate::getDisplayInfo(const QModelIndex& index, QString& baseName, QString& extInfo) const
{
    baseName = index.data(Qt::DisplayRole).toString();
    extInfo = QString();

    static const QString kCustomExtInfoTemplate = //< "- %1" with en-dash
        QString::fromWCharArray(L"\x2013 %1");

    /* Two-component text from resource information: */
    auto infoLevel = m_customInfoLevel;
    if (infoLevel == Qn::RI_Invalid)
        infoLevel = qnSettings->extraInfoInTree();
    if (infoLevel == Qn::RI_NameOnly)
        return;

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();

    if (nodeType == Qn::VideoWallItemNode)
    {
        // skip videowall screens
    }
    else if (nodeType == Qn::RecorderNode)
    {
        auto firstChild = index.model()->index(0, 0, index);
        if (!firstChild.isValid()) /* This can happen in rows deleting */
            return;
        QnResourcePtr resource = firstChild.data(Qn::ResourceRole).value<QnResourcePtr>();
        extInfo = QnResourceDisplayInfo(resource).extraInfo();
    }
    else
    {
        if (!resource)
            return;

        if ((nodeType == Qn::LayoutItemNode || nodeType == Qn::SharedResourceNode)
            && resource->hasFlags(Qn::server)
            && !resource->hasFlags(Qn::fake))
        {
            extInfo = kCustomExtInfoTemplate.arg(tr("Health Monitor"));
        }
        else
        {
            QnResourceDisplayInfo info(resource);
            extInfo = info.extraInfo();
        }

        if (resource->hasFlags(Qn::user) && !extInfo.isEmpty())
            extInfo = kCustomExtInfoTemplate.arg(extInfo);

        if (resource->hasFlags(Qn::wearable_camera))
        {
            QnSecurityCamResourcePtr camera = resource.dynamicCast<QnSecurityCamResource>();

            using namespace nx::client::desktop;
            WearableState state = qnClientModule->wearableManager()->state(camera);
            if (state.isRunning())
                extInfo = kCustomExtInfoTemplate.arg(QString::number(state.progress()) + lit("%"));
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
