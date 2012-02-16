#include "action_meta_types.h"
#include <core/resource/resource_fwd.h>
#include <ui/graphics/items/resource_widget.h>

namespace {
    int qn_graphicsItemListMetaType = 0;
    int qn_resourceListMetaType = 0;

    Q_GLOBAL_STATIC_WITH_INITIALIZER(bool, qn_initializeActionMetaTypes, {
        qn_graphicsItemListMetaType = qMetaTypeId<QGraphicsItemList>();
        qn_resourceListMetaType = qMetaTypeId<QnResourceList>();
    });

} // anonymous namespace

int QnActionMetaTypes::resourceList() {
    return qn_resourceListMetaType;
}

int QnActionMetaTypes::graphicsItemList() {
    return qn_graphicsItemListMetaType;
}

void QnActionMetaTypes::initialize() {
    qn_initializeActionMetaTypes();
}

int QnActionMetaTypes::size(const QVariant &items) {
    if(items.userType() == resourceList()) {
        return items.value<QnResourceList>().size();
    } else if(items.userType() == graphicsItemList()) {
        return items.value<QGraphicsItemList>().size();
    } else {
        return 0;
    }
}

QnResourcePtr QnActionMetaTypes::resource(QGraphicsItem *item) {
    if(item == NULL)
        return QnResourcePtr();

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget == NULL)
        return QnResourcePtr();

    return widget->resource();
}

QnResourceList QnActionMetaTypes::resources(const QGraphicsItemList &items) {
    QnResourceList result;

    foreach(QGraphicsItem *item, items) {
        QnResourcePtr resource = QnActionMetaTypes::resource(item);
        if(!resource)
            continue;

        result.push_back(resource);
    }

    return result;
}

QnResourceList QnActionMetaTypes::resources(const QVariant &items) {
    if(items.userType() == qn_resourceListMetaType) {
        return items.value<QnResourceList>();
    } else if(items.userType() == qn_graphicsItemListMetaType) {
        return resources(items.value<QGraphicsItemList>());
    } else {
        return QnResourceList();
    }
}

QGraphicsItemList QnActionMetaTypes::graphicsItems(const QVariant &items) {
    if(items.userType() == qn_graphicsItemListMetaType) {
        return items.value<QGraphicsItemList>();
    } else {
        return QGraphicsItemList();
    }
}