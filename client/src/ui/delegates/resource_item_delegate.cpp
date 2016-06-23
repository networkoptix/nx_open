#include "resource_item_delegate.h"

#include <QtWidgets/QApplication>

#include <core/resource/camera_history.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/resource_display_info.h>

#include <client/client_meta_types.h>
#include <client/client_settings.h>

#include <ui/style/skin.h>
#include <ui/style/helper.h>
#include <ui/style/noptix_style.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/common/scoped_painter_rollback.h>

#include <common/common_module.h>

QnResourceItemDelegate::QnResourceItemDelegate(QObject* parent):
    base_type(parent),
    m_workbench(),
    m_recordingIcon(qnSkin->icon("tree/recording.png")),
    m_scheduledIcon(qnSkin->icon("tree/scheduled.png")),
    m_buggyIcon(qnSkin->icon("tree/buggy.png")),
    m_colors(),
    m_fixedHeight(style::Metrics::kViewRowHeight),
    m_rowSpacing(0),
    m_customInfoLevel(Qn::ResourceInfoLevel::RI_Invalid)
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

    /* Check indicators in this implementation are handled elsewhere: */
    if (option.features.testFlag(QStyleOptionViewItem::HasCheckIndicator)) // TODO #vkutin Get rid of this and draw checkboxes in this delegate like everything else
    {
        base_type::paint(painter, option, index);
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
    case ItemState::Selected:
        iconMode = QIcon::Selected;
        mainColor = m_colors.mainTextSelected;
        extraColor = m_colors.extraTextSelected;
        break;

    case ItemState::Accented:
        iconMode = QIcon::Active;
        mainColor = m_colors.mainTextAccented;
        extraColor = m_colors.extraTextAccented;
        break;

    default:
        iconMode = QIcon::Normal;
        mainColor = m_colors.mainText;
        extraColor = m_colors.extraText;
    }

    if (index.column() == Qn::CustomColumn)
    {
        QVariant customColor = index.data(Qt::ForegroundRole);
        if (!customColor.isNull() && customColor.canConvert<QColor>())
            mainColor = customColor.value<QColor>();
    };

    /* Due to Qt bug, State_Editing is not set in option.state, so detect editing differently: */
    const QAbstractItemView* view = qobject_cast<const QAbstractItemView*>(option.widget);
    bool editing = view && view->indexWidget(option.index);

    /* Obtain sub-element rectangles: */
    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
    QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &option, option.widget);

    /* Paint background: */
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    /* Set opacity if disabled: */
    QnScopedPainterOpacityRollback opacityRollback(painter);
    if (!option.state.testFlag(QStyle::State_Enabled))
        painter->setOpacity(painter->opacity() * 0.3);

    /* Draw icon: */
    if (option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        option.icon.paint(painter, iconRect, option.decorationAlignment, iconMode, QIcon::On);

    /* Draw text: */
    if (!editing)
    {
        QString baseName;
        QString extraInfo;
        getDisplayInfo(index, baseName, extraInfo);

        QnScopedPainterFontRollback fontRollback(painter, option.font);
        QnScopedPainterPenRollback penRollback(painter, mainColor);

        const int textFlags = Qt::TextSingleLine | Qt::TextHideMnemonic | option.displayAlignment;
        const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; /* As in Qt */
        textRect.adjust(textPadding, 0, -textPadding, 0);

        QString elidedName = option.fontMetrics.elidedText(baseName, option.textElideMode, textRect.width(), textFlags);

        QRect actualRect;
        painter->drawText(textRect, textFlags, elidedName, &actualRect);

        if (elidedName == baseName && !extraInfo.isEmpty())
        {
            option.font.setWeight(QFont::Normal);
            QFontMetrics extraMetrics(option.font);

            /* If name was empty, actualRect will be invalid: */
            int startPos = actualRect.isValid() ? actualRect.right() : textRect.left();

            textRect.setLeft(startPos + textPadding * 2);
            QString elidedHost = extraMetrics.elidedText(extraInfo, option.textElideMode, textRect.width(), textFlags);

            painter->setFont(option.font);
            painter->setPen(extraColor);
            painter->drawText(textRect, textFlags, elidedHost);
        }
    }

    /* Draw "recording" or "scheduled" icon: */
    QRect extraIconRect = iconRect.adjusted(-4, 0, -4, 0);
    extraIconRect.moveLeft(extraIconRect.left() - extraIconRect.width());

    bool recording = false;
    bool scheduled = false;
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (resource)
    {
        if (resource->getStatus() == Qn::Recording && resource.dynamicCast<QnVirtualCameraResource>())
            recording = true;

        if (!recording)
            if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
                scheduled = !camera->isScheduleDisabled();
    }

    if (recording || scheduled)
        (recording ? m_recordingIcon : m_scheduledIcon).paint(painter, extraIconRect);

    /* Draw "problems" icon: */
    extraIconRect.moveLeft(extraIconRect.left() - extraIconRect.width());
    if (QnSecurityCamResourcePtr camera = resource.dynamicCast<QnSecurityCamResource>())
        if (camera->statusFlags().testFlag(Qn::CSF_HasIssuesFlag))
            m_buggyIcon.paint(painter, extraIconRect);
}

QSize QnResourceItemDelegate::sizeHint(const QStyleOptionViewItem& styleOption, const QModelIndex& index) const
{
    /* Initialize style option: */
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    /* If item is separator, return separator size: */
    if (option.features == QStyleOptionViewItem::None)
        return style::Metrics::kSeparatorSize + QSize(0, m_rowSpacing);

    // TODO #vkutin Keep this while checkboxed items are painted by default implementation:
    if (option.features.testFlag(QStyleOptionViewItem::HasCheckIndicator))
        return base_type::sizeHint(option, index);

    QStyle* style = option.widget ? option.widget->style() : QApplication::style();

    /* Let the style calculate text offset: */
    option.rect.setSize(QSize(10000, 20)); // any nonzero height and some really big width
    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);

    /* Initial size: */
    int width = textRect.left();
    int height = option.decorationSize.height();

    /* Adjust size to text: */
    {
        QString baseName;
        QString extraInfo;
        getDisplayInfo(index, baseName, extraInfo);

        const int kTextFlags = Qt::TextSingleLine | Qt::TextHideMnemonic;
        int leftRightPadding = (style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget) + 1) * 2; // As in Qt

        /* Adjust height to text: */
        height = qMax(height, option.fontMetrics.height());

        /* Width of the main text: */
        width += option.fontMetrics.width(baseName, -1, kTextFlags);

        if (!extraInfo.isEmpty())
        {
            /* Width of the extra text: */
            option.font.setWeight(QFont::Normal);
            QFontMetrics metrics(option.font);
            width += option.fontMetrics.width(extraInfo, -1, kTextFlags) + leftRightPadding;
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
    if (!view || !view->iconSize().isValid())
    {
        enum { kMaxIconSize = 32 };
        int maxSize = m_fixedHeight > 0 ? m_fixedHeight : kMaxIconSize;
        option->decorationSize = QSize(maxSize, maxSize);
    }

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
}

QWidget* QnResourceItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    /* Create editor: */
    QWidget* editor = base_type::createEditor(parent, option, index);

    /* Change editor font: */
    QFont font(editor->font());
    font.setWeight(QFont::DemiBold);
    editor->setFont(font);

    /* Set text and selected text editor colors by item state: */
    QPalette editorPalette = editor->palette();
    switch (itemState(index))
    {
    case ItemState::Normal:
        editorPalette.setColor(QPalette::Text, m_colors.mainText);
        editorPalette.setColor(QPalette::HighlightedText, m_colors.mainText);
        break;

    case ItemState::Selected:
        editorPalette.setColor(QPalette::Text, m_colors.mainTextSelected);
        editorPalette.setColor(QPalette::HighlightedText, m_colors.mainTextSelected);
        break;

    case ItemState::Accented:
        editorPalette.setColor(QPalette::Text, m_colors.mainTextAccented);
        editorPalette.setColor(QPalette::HighlightedText, m_colors.mainTextAccented);
        break;
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
    Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();
    if (nodeType == Qn::CurrentSystemNode)
        return ItemState::Selected;

    /* Fetch information from model: */
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (workbench() && resource == workbench()->context()->user())
        return ItemState::Selected;

    QnResourcePtr currentLayoutResource = workbench() ? workbench()->currentLayout()->resource() : QnLayoutResourcePtr();
    QnResourcePtr parentResource = index.parent().data(Qn::ResourceRole).value<QnResourcePtr>();
    QnUuid uuid = index.data(Qn::ItemUuidRole).value<QnUuid>();


    /* Determine central workbench item: */
    QnWorkbenchItem* centralItem = workbench() ? workbench()->item(Qn::CentralRole) : nullptr;

    if (centralItem && (centralItem->uuid() == uuid || (resource && uuid.isNull() && centralItem->resourceUid() == resource->getUniqueId())))
    {
        /* Central item: */
        return ItemState::Accented;
    }
    else if (!resource.isNull() && !currentLayoutResource.isNull())
    {
        bool videoWallControlMode = workbench() ? !workbench()->currentLayout()->data(Qn::VideoWallItemGuidRole).value<QnUuid>().isNull() : false;
        if (resource == currentLayoutResource)
        {
            /* Current layout if we aren't in videowall control mode or current videowall if we are: */
            if (videoWallControlMode != uuid.isNull())
                return ItemState::Selected;
        }
        else if (parentResource == currentLayoutResource ||
            (uuid.isNull() && workbench() && !workbench()->currentLayout()->items(resource->getUniqueId()).isEmpty()))
        {
            /* Item on current layout. */
            return ItemState::Selected;
        }
    }

    /* Normal item: */
    return ItemState::Normal;
}

void QnResourceItemDelegate::getDisplayInfo(const QModelIndex& index, QString& baseName, QString& extInfo) const
{
    baseName = index.data(Qt::DisplayRole).toString();
    if (index.column() > Qn::NameColumn)
        return;

    static const QString kCustomExtInfoTemplate = lit(" - %1");

    /* Two-component text from resource information: */
    auto infoLevel = m_customInfoLevel;
    if (infoLevel == Qn::RI_Invalid)
        infoLevel = qnSettings->extraInfoInTree();
    if (infoLevel == Qn::RI_NameOnly)
        return;

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (!resource)
        return;

    Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();

    if (nodeType == Qn::VideoWallItemNode)
    {
        extInfo = kCustomExtInfoTemplate.arg(resource->getName());
    }
    else
    {
        QnResourceDisplayInfo info(resource);
        extInfo = info.extraInfo();

        if (resource->hasFlags(Qn::user) && !extInfo.isEmpty())
            extInfo = kCustomExtInfoTemplate.arg(extInfo);
    }
}
