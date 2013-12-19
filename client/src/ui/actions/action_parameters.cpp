#include "action_parameters.h"

#include <utils/common/warnings.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include "action_parameter_types.h"

namespace {
    bool checkType(const QVariant &items) {
        Qn::ActionParameterType type = QnActionParameterTypes::type(items);
        if(type == Qn::OtherType) {
            qnWarning("Unrecognized action parameter type '%1'.", items.typeName());
            return false;
        }

        return true;
    }

} // anonymous namespace

QnActionParameters::QnActionParameters(const ArgumentHash &arguments) {
    setArguments(arguments);
    setItems(QVariant::fromValue<QnResourceList>(QnResourceList()));
}

QnActionParameters::QnActionParameters(const QVariant &items, const ArgumentHash &arguments) {
    setArguments(arguments);
    setItems(items);
}

QnActionParameters::QnActionParameters(const QList<QGraphicsItem *> &items, const ArgumentHash &arguments) {
    setArguments(arguments);
    setItems(QVariant::fromValue<QnResourceWidgetList>(QnActionParameterTypes::widgets(items)));
}

QnActionParameters::QnActionParameters(QnResourceWidget *widget, const ArgumentHash &arguments) {
    setArguments(arguments);
    setItems(QVariant::fromValue<QnResourceWidget *>(widget));
}

QnActionParameters::QnActionParameters(const QnResourceWidgetList &widgets, const ArgumentHash &arguments) {
    setArguments(arguments);
    setItems(QVariant::fromValue<QnResourceWidgetList>(widgets));
}

QnActionParameters::QnActionParameters(const QnWorkbenchLayoutList &layouts, const ArgumentHash &arguments) {
    setArguments(arguments);
    setItems(QVariant::fromValue<QnWorkbenchLayoutList>(layouts));
}

QnActionParameters::QnActionParameters(const QnLayoutItemIndexList &layoutItems, const ArgumentHash &arguments) {
    setArguments(arguments);
    setItems(QVariant::fromValue<QnLayoutItemIndexList>(layoutItems));
}

void QnActionParameters::setArguments(const ArgumentHash &arguments) {
    for(ArgumentHash::const_iterator pos = arguments.begin(); pos != arguments.end(); pos++)
        setArgument(pos.key(), pos.value());
}

void QnActionParameters::setArgument(int key, const QVariant &value) {
    if(key == -1 && !checkType(value))
        return;

    m_arguments[key] = value;
}

void QnActionParameters::setItems(const QVariant &items) {
    setArgument(-1, items);
}

void QnActionParameters::setResources(const QnResourceList &resources) {
    setItems(QVariant::fromValue<QnResourceList>(resources));
}

Qn::ActionParameterType QnActionParameters::type(int key) const {
    return QnActionParameterTypes::type(argument(key));
}

int QnActionParameters::size(int key) const {
    return QnActionParameterTypes::size(argument(key));
}

QnResourceList QnActionParameters::resources(int key) const {
    return QnActionParameterTypes::resources(argument(key));
}

QnResourcePtr QnActionParameters::resource(int key) const {
    QnResourceList resources = this->resources(key);

    if(resources.size() != 1)
        qnWarning("Invalid number of target resources: expected %2, got %3.", 1, resources.size());

    return resources.isEmpty() ? QnResourcePtr() : resources.front();
}

QnLayoutItemIndexList QnActionParameters::layoutItems(int key) const {
    return QnActionParameterTypes::layoutItems(argument(key));
}

QnWorkbenchLayoutList QnActionParameters::layouts(int key) const {
    return QnActionParameterTypes::layouts(argument(key));
}

QnResourceWidget *QnActionParameters::widget(int key) const {
    QnResourceWidgetList widgets = this->widgets(key);

    if(widgets.size() != 1)
        qnWarning("Invalid number of target widgets: expected %2, got %3.", 1, widgets.size());

    return widgets.isEmpty() ? NULL : widgets.front();
}

QnResourceWidgetList QnActionParameters::widgets(int key) const {
    return QnActionParameterTypes::widgets(argument(key));
}


