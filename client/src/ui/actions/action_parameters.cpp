#include "action_parameters.h"

#include <core/resource/resource.h>
#include <core/resource/layout_item_index.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>

#include <utils/common/warnings.h>

#include <ui/workbench/workbench_layout.h>
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
    init(QVariant::fromValue<QnResourceList>(QnResourceList()), arguments);
}

QnActionParameters::QnActionParameters(const QVariant &items, const ArgumentHash &arguments) {
    init(items, arguments);
}

QnActionParameters::QnActionParameters(const QList<QGraphicsItem *> &items, const ArgumentHash &arguments) {
    init(QVariant::fromValue<QnResourceWidgetList>(QnActionParameterTypes::widgets(items)), arguments);
}

QnActionParameters::QnActionParameters(QnResourceWidget *widget, const ArgumentHash &arguments) {
    init(QVariant::fromValue<QnResourceWidget *>(widget), arguments);
}

QnActionParameters::QnActionParameters(const QnResourceWidgetList &widgets, const ArgumentHash &arguments) {
    init(QVariant::fromValue<QnResourceWidgetList>(widgets), arguments);
}

QnActionParameters::QnActionParameters(const QnWorkbenchLayoutList &layouts, const ArgumentHash &arguments) {
    init(QVariant::fromValue<QnWorkbenchLayoutList>(layouts), arguments);
}

QnActionParameters::QnActionParameters(const QnLayoutItemIndexList &layoutItems, const ArgumentHash &arguments) {
    init(QVariant::fromValue<QnLayoutItemIndexList>(layoutItems), arguments);
}

QnActionParameters::QnActionParameters(const QnVideoWallItemIndexList &videoWallItems, const ArgumentHash &arguments) {
    init(QVariant::fromValue<QnVideoWallItemIndexList>(videoWallItems), arguments);
}

QnActionParameters::QnActionParameters(const QnVideoWallMatrixIndexList &videoWallMatrices, const ArgumentHash &arguments) {
    init(QVariant::fromValue<QnVideoWallMatrixIndexList>(videoWallMatrices), arguments);
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

QnVideoWallItemIndexList QnActionParameters::videoWallItems(int key) const {
    return QnActionParameterTypes::videoWallItems(argument(key));
}

QnVideoWallMatrixIndexList QnActionParameters::videoWallMatrices(int key) const {
    return QnActionParameterTypes::videoWallMatrices(argument(key));
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

Qn::ActionScopes QnActionParameters::scope() const {
    return m_scope;
}

void QnActionParameters::setScope(Qn::ActionScopes scope) {
    m_scope = scope;
}

void QnActionParameters::init(const QVariant &items, const ArgumentHash &arguments) {
    setScope(Qn::InvalidScope);
    setArguments(arguments);
    setItems(items);
}
