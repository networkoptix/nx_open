#include "resource_tree_item_delegate.h"

#include <core/resource/camera_history.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>

#include <client/client_meta_types.h>

#include <ui/style/skin.h>
#include <ui/style/noptix_style.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>

QnResourceTreeItemDelegate::QnResourceTreeItemDelegate(QObject *parent):
    base_type(parent)
{
    m_recordingIcon = qnSkin->icon("tree/recording.png");
    m_raisedIcon = qnSkin->icon("tree/raised.png");
    m_scheduledIcon = qnSkin->icon("tree/scheduled.png");
}

void QnResourceTreeItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QStyleOptionViewItemV4 optionV4 = option;
    initStyleOption(&optionV4, index);

    if(optionV4.widget && optionV4.widget->rect().bottom() < optionV4.rect.bottom()
            && optionV4.widget->property(Qn::HideLastRowInTreeIfNotEnoughSpace).toBool())
        return;

    if (index.column() == Qn::CheckColumn) {
        base_type::paint(painter, option, index);
        return;
    }

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    QnResourcePtr currentLayoutResource = workbench() ? workbench()->currentLayout()->resource() : QnLayoutResourcePtr();
    QnResourcePtr parentResource = index.parent().data(Qn::ResourceRole).value<QnResourcePtr>();
    QUuid uuid = index.data(Qn::ItemUuidRole).value<QUuid>();

    /* Bold items of current layout in tree. */
    if(!resource.isNull() && !currentLayoutResource.isNull()) {
        bool bold = false;
        if(resource == currentLayoutResource) {
            bold = true; /* Bold current layout. */
        } else if(parentResource == currentLayoutResource) {
            bold = true; /* Bold items of the current layout. */
        } else if(uuid.isNull() && workbench() && !workbench()->currentLayout()->items(resource->getUniqueId()).isEmpty()) {
            bold = true; /* Bold items of the current layout in servers. */
        }

        optionV4.font.setBold(bold);
    }

    QStyle *style = optionV4.widget ? optionV4.widget->style() : QApplication::style();

    /* Highlight currently raised/zoomed item. */
    QnWorkbenchItem *raisedItem = NULL;
    if(workbench()) {
        raisedItem = workbench()->item(Qn::RaisedRole);
        if(!raisedItem)
            raisedItem = workbench()->item(Qn::ZoomedRole);
    }

    QRect decorationRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &optionV4, optionV4.widget);

    if(raisedItem && (raisedItem->uuid() == uuid || (resource && uuid.isNull() && raisedItem->resourceUid() == resource->getUniqueId()))) {
        m_raisedIcon.paint(painter, decorationRect);

        QRect rect = optionV4.rect;
        QRect skipRect(
            rect.topLeft(),
            QPoint(
                decorationRect.right() + decorationRect.left() - rect.left(),
                rect.bottom()
            )
        );
        rect.setLeft(skipRect.right() + 1);

        optionV4.rect = skipRect;
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &optionV4, painter, optionV4.widget);
        optionV4.rect = rect;
    }

    /* Draw 'recording' icon. */
    bool recording = false, scheduled = false;
    if(resource) {
        if(!resource->isDisabled()) {
            if(resource->getStatus() == QnResource::Recording && resource.dynamicCast<QnVirtualCameraResource>())
                recording = true;
        } else if(QnNetworkResourcePtr camera = resource.dynamicCast<QnNetworkResource>()) {
            foreach(const QnNetworkResourcePtr &otherCamera, QnCameraHistoryPool::instance()->getAllCamerasWithSamePhysicalId(camera)) {
                if(!otherCamera->isDisabled() && otherCamera->getStatus() == QnResource::Recording) {
                    recording = true;
                    break;
                }
            }
        }

        if(!recording)
            if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
                scheduled = !camera->isScheduleDisabled();
    }

    if(recording || scheduled) {
        QRect iconRect = decorationRect;
        iconRect.moveLeft(iconRect.left() - iconRect.width() - 2);

        (recording ? m_recordingIcon : m_scheduledIcon).paint(painter, iconRect);
    }

    /* Draw item. */
    style->drawControl(QStyle::CE_ItemViewItem, &optionV4, painter, optionV4.widget);
}

void QnResourceTreeItemDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const {
    base_type::initStyleOption(option, index);
}
