#include "resource_tree_item_delegate.h"

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

QnResourceTreeItemDelegate::QnResourceTreeItemDelegate(QObject* parent):
    base_type(parent)
{
    m_recordingIcon = qnSkin->icon("tree/recording.png");
    m_scheduledIcon = qnSkin->icon("tree/scheduled.png");
    m_buggyIcon = qnSkin->icon("tree/buggy.png");
}

void QnResourceTreeItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& styleOption, const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    if (option.widget && option.widget->rect().bottom() < option.rect.bottom() &&
        option.widget->property(Qn::HideLastRowInTreeIfNotEnoughSpace).toBool())
    {
        return;
    }

    if (index.column() == Qn::CheckColumn) // TODO #vkutin Get rid of this and draw checkboxes in this delegate like everything else
    {
        base_type::paint(painter, option, index);
        return;
    }

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    QnResourcePtr currentLayoutResource = workbench() ? workbench()->currentLayout()->resource() : QnLayoutResourcePtr();
    QnResourcePtr parentResource = index.parent().data(Qn::ResourceRole).value<QnResourcePtr>();
    QnUuid uuid = index.data(Qn::ItemUuidRole).value<QnUuid>();

    /* Determine currently raised/zoomed item. */
    QnWorkbenchItem* raisedItem = nullptr;
    if (workbench())
    {
        raisedItem = workbench()->item(Qn::RaisedRole);
        if (!raisedItem)
            raisedItem = workbench()->item(Qn::ZoomedRole);
    }

    QPalette::ColorRole actualRole = QPalette::Text;
    QIcon::Mode iconMode = QIcon::Normal;

    if (raisedItem && (raisedItem->uuid() == uuid || (resource && uuid.isNull() && raisedItem->resourceUid() == resource->getUniqueId())))
    {
        /* Brighten raised/zoomed item. */
        actualRole = QPalette::BrightText;
        iconMode = QIcon::Active;
    }
    else if (!resource.isNull() && !currentLayoutResource.isNull())
    {
        bool videoWallControlMode = workbench() ? !workbench()->currentLayout()->data(Qn::VideoWallItemGuidRole).isNull() : false;
        if (resource == currentLayoutResource)
        {
            /* Highlight current layout if we aren't in videowall control mode or current videowall if we are. */
            if (videoWallControlMode != uuid.isNull())
            {
                actualRole = QPalette::HighlightedText;
                iconMode = QIcon::Selected;
            }
        }
        else if (parentResource == currentLayoutResource ||
            (uuid.isNull() && workbench() && !workbench()->currentLayout()->items(resource->getUniqueId()).isEmpty()))
        {
            /* Highlight Brighten items on current layout. */
            actualRole = QPalette::HighlightedText;
            iconMode = QIcon::Selected;
        }
    }

    QStyle* style = option.widget ? option.widget->style() : QApplication::style();

    /* Obtain sub-element rectangles: */
    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
    QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &option, option.widget);

    /* Check indicators in this implementation are handled elsewhere: */
    NX_ASSERT(!option.features.testFlag(QStyleOptionViewItem::HasCheckIndicator));

    /* Paint background: */
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    /* Draw icon: */
    if (option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        option.icon.paint(painter, iconRect, option.decorationAlignment, iconMode, QIcon::On);

    /* Draw text: */
    bool extraResourceInfo = qnSettings->isIpShownInTree() && !resource.isNull();
    if (option.features.testFlag(QStyleOptionViewItem::HasDisplay))
    {
        option.font.setWeight(QFont::DemiBold);

        QnScopedPainterFontRollback fontRollback(painter, option.font);
        QnScopedPainterPenRollback penRollback(painter, option.palette.color(QPalette::Normal, actualRole));

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

                textRect.setLeft(actualRect.right() + kTextPadding*2);
                QString elidedHost = extraMetrics.elidedText(host, option.textElideMode, textRect.width(), kTextFlags);

                painter->setFont(option.font);
                painter->setPen(option.palette.color(QPalette::Inactive, actualRole));
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

QSize QnResourceTreeItemDelegate::sizeHint(const QStyleOptionViewItem& styleOption, const QModelIndex& index) const
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

void QnResourceTreeItemDelegate::destroyEditor(QWidget* editor, const QModelIndex& index) const
{
    // TODO: #QTBUG If focus is not cleared, we get crashes. Should be fixed in QWidget.
    editor->clearFocus();
    base_type::destroyEditor(editor, index);
}

bool QnResourceTreeItemDelegate::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::ContextMenu)
        return true; /* Ignore context menu events as they don't work well with editors embedded into graphics scene. */

    return base_type::eventFilter(object, event);
}

