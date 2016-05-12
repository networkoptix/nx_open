#include "resource_item_delegate.h"

#include <QtWidgets/QApplication>

#include <core/resource/camera_history.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource_name.h>

#include <client/client_meta_types.h>
#include <client/client_settings.h>

#include <ui/style/skin.h>
#include <ui/style/noptix_style.h>
#include <ui/style/helper.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/common/scoped_painter_rollback.h>

#include <common/common_module.h>

QnResourceItemDelegate::QnResourceItemDelegate(QObject* parent):
    base_type(parent)
{
    m_recordingIcon = qnSkin->icon("tree/recording.png");
    m_scheduledIcon = qnSkin->icon("tree/scheduled.png");
    m_buggyIcon = qnSkin->icon("tree/buggy.png");
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

    QStyle* style = option.widget ? option.widget->style() : QApplication::style();

    /* Due to Qt bug, State_Editing is not set in option.state, so detect editing differently: */
    const QAbstractItemView* view = qobject_cast<const QAbstractItemView*>(option.widget);
    bool editing = view && view->indexWidget(option.index);

    /* Obtain sub-element rectangles: */
    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
    QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &option, option.widget);

    /* Paint background: */
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    /* Draw icon: */
    if (option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        option.icon.paint(painter, iconRect, option.decorationAlignment, iconMode, QIcon::On);

    /* Get resource from the model: */
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    bool extraResourceInfo = qnSettings->isIpShownInTree() && !resource.isNull();

    /* Draw text: */
    if (option.features.testFlag(QStyleOptionViewItem::HasDisplay) && !editing)
    {
        option.font.setWeight(QFont::DemiBold);

        QnScopedPainterFontRollback fontRollback(painter, option.font);
        QnScopedPainterPenRollback penRollback(painter, mainColor);

        QFontMetrics metrics(option.font);
        const int kTextFlags = Qt::TextSingleLine | Qt::TextHideMnemonic | option.displayAlignment;
        const int kTextPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; /* As in Qt */
        textRect.adjust(kTextPadding, 0, -kTextPadding, 0);

        if (extraResourceInfo)
        {
            QString name, host;
            getResourceDisplayInformation(resource, name, host);

            QRect actualRect;
            QString elidedName = metrics.elidedText(name, option.textElideMode, textRect.width(), kTextFlags);

            painter->drawText(textRect, kTextFlags, elidedName, &actualRect);

            if (elidedName == name && !host.isEmpty())
            {
                option.font.setWeight(QFont::Normal);
                QFontMetrics extraMetrics(option.font);

                /* If name was empty, actualRect will be invalid: */
                int startPos = actualRect.isValid() ? actualRect.right() : textRect.left();

                textRect.setLeft(startPos + kTextPadding*2);
                QString elidedHost = extraMetrics.elidedText(host, option.textElideMode, textRect.width(), kTextFlags);

                painter->setFont(option.font);
                painter->setPen(extraColor);
                painter->drawText(textRect, kTextFlags, elidedHost);
            }
        }
        else
        {
            QString elidedText = metrics.elidedText(option.text, option.textElideMode, textRect.width(), kTextFlags);
            painter->drawText(textRect, kTextFlags, elidedText);
        }
    }

    /* Draw "recording" or "scheduled" icon: */
    QRect extraIconRect = iconRect.adjusted(-4, 0, -4, 0);
    extraIconRect.moveLeft(extraIconRect.left() - extraIconRect.width());

    bool recording = false, scheduled = false;
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
        if (camera->statusFlags() & Qn::CSF_HasIssuesFlag)
            m_buggyIcon.paint(painter, extraIconRect);
}

QSize QnResourceItemDelegate::sizeHint(const QStyleOptionViewItem& styleOption, const QModelIndex& index) const
{
    /* Init style option: */
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    /* Obtain sub-elements layout: */
    QStyle* style = option.widget ? option.widget->style() : QApplication::style();
    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
    QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &option, option.widget);
    QRect checkRect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &option, option.widget);

    /* Return minimal required size: */
    return (textRect | iconRect | checkRect).size();
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
    /* Fetch information from model: */
    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
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
