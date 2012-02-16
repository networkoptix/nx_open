#include "action_meta_types.h"
#include <core/resource/resource_fwd.h>
#include <ui/graphics/items/resource_widget.h>

namespace {
    int qn_widgetListMetaType = 0;
    int qn_resourceListMetaType = 0;

    Q_GLOBAL_STATIC_WITH_INITIALIZER(bool, qn_initializeActionMetaTypes, {
        qn_widgetListMetaType = qMetaTypeId<QnResourceWidgetList>();
        qn_resourceListMetaType = qMetaTypeId<QnResourceList>();
    });

} // anonymous namespace

int QnActionMetaTypes::resourceList() {
    return qn_resourceListMetaType;
}

int QnActionMetaTypes::widgetList() {
    return qn_widgetListMetaType;
}

void QnActionMetaTypes::initialize() {
    qn_initializeActionMetaTypes();
}

int QnActionMetaTypes::size(const QVariant &items) {
    if(items.userType() == resourceList()) {
        return items.value<QnResourceList>().size();
    } else if(items.userType() == widgetList()) {
        return items.value<QnResourceWidgetList>().size();
    } else {
        return 0;
    }
}

QnResourcePtr QnActionMetaTypes::resource(QnResourceWidget *widget) {
    if(widget == NULL)
        return QnResourcePtr();

    return widget->resource();
}

QnResourceList QnActionMetaTypes::resources(const QnResourceWidgetList &widgets) {
    QnResourceList result;

    foreach(QnResourceWidget *widget, widgets) {
        QnResourcePtr resource = QnActionMetaTypes::resource(widget);
        if(!resource)
            continue;

        result.push_back(resource);
    }

    return result;
}

QnResourceList QnActionMetaTypes::resources(const QVariant &items) {
    if(items.userType() == qn_resourceListMetaType) {
        return items.value<QnResourceList>();
    } else if(items.userType() == qn_widgetListMetaType) {
        return resources(items.value<QnResourceWidgetList>());
    } else {
        return QnResourceList();
    }
}

QnResourceWidgetList QnActionMetaTypes::widgets(const QVariant &items) {
    if(items.userType() == qn_widgetListMetaType) {
        return items.value<QnResourceWidgetList>();
    } else {
        return QnResourceWidgetList();
    }
}

QnResourceWidgetList QnActionMetaTypes::widgets(const QList<QGraphicsItem *> items) {
    QnResourceWidgetList result;

    foreach(QGraphicsItem *item, items) {
        QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
        if(widget == NULL)
            continue;

        result.push_back(widget);
    }

    return result;
}