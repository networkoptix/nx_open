#include "action_parameters.h"

#include <utils/common/warnings.h>

#include "action_target_types.h"

namespace {
    bool checkType(const QVariant &items) {
        Qn::ActionTargetType type = QnActionTargetTypes::type(items);
        if(type == 0) {
            qnWarning("Unrecognized action target type '%1'.", items.typeName());
            return false;
        }

        return true;
    }

} // anonymous namespace

QnActionParameters::QnActionParameters(const QVariantMap &arguments):
    m_items(QVariant::fromValue<QnResourceList>(QnResourceList())),
    m_arguments(arguments)
{}

QnActionParameters::QnActionParameters(const QVariant &items, const QVariantMap &arguments) {
    setItems(items);
    setArguments(arguments);
}

QnActionParameters::QnActionParameters(const QnResourcePtr &resource, const QVariantMap &arguments) {
    QnResourceList resources;
    resources.push_back(resource);

    setItems(QVariant::fromValue<QnResourceList>(resources));
    setArguments(arguments);
}

QnActionParameters::QnActionParameters(const QnResourceList &resources, const QVariantMap &arguments) {
    setItems(QVariant::fromValue<QnResourceList>(resources));
    setArguments(arguments);
}

QnActionParameters::QnActionParameters(const QList<QGraphicsItem *> &items, const QVariantMap &arguments) {
    setItems(QVariant::fromValue<QnResourceWidgetList>(QnActionTargetTypes::widgets(items)));
    setArguments(arguments);
}

QnActionParameters::QnActionParameters(const QnResourceWidgetList &widgets, const QVariantMap &arguments) {
    setItems(QVariant::fromValue<QnResourceWidgetList>(widgets));
    setArguments(arguments);
}

QnActionParameters::QnActionParameters(const QnWorkbenchLayoutList &layouts, const QVariantMap &arguments) {
    setItems(QVariant::fromValue<QnWorkbenchLayoutList>(layouts));
    setArguments(arguments);
}

QnActionParameters::QnActionParameters(const QnLayoutItemIndexList &layoutItems, const QVariantMap &arguments) {
    setItems(QVariant::fromValue<QnLayoutItemIndexList>(layoutItems));
    setArguments(arguments);
}
    
void QnActionParameters::setItems(const QVariant &items) {
    if(checkType(items))
        m_items = items;
}

Qn::ActionTargetType QnActionParameters::itemsType() const {
    return QnActionTargetTypes::type(m_items);
}

int QnActionParameters::itemsSize() const {
    return QnActionTargetTypes::size(m_items);
}

QnResourceList QnActionParameters::resources() const {
    return QnActionTargetTypes::resources(m_items);
}

QnResourcePtr QnActionParameters::resource() const {
    QnResourceList resources = this->resources();

    if(resources.size() != 1)
        qnWarning("Invalid number of target resources: expected %2, got %3.", 1, resources.size());

    return resources.isEmpty() ? QnResourcePtr() : resources.front();
}

QnLayoutItemIndexList QnActionParameters::layoutItems() const {
    return QnActionTargetTypes::layoutItems(m_items);
}

QnWorkbenchLayoutList QnActionParameters::layouts() const {
    return QnActionTargetTypes::layouts(m_items);
}

QnResourceWidget *QnActionParameters::widget() const {
    QnResourceWidgetList widgets = this->widgets();

    if(widgets.size() != 1)
        qnWarning("Invalid number of target widgets: expected %2, got %3.", 1, widgets.size());

    return widgets.isEmpty() ? NULL : widgets.front();
}

QnResourceWidgetList QnActionParameters::widgets() const {
    return QnActionTargetTypes::widgets(m_items);
}



