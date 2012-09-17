#include "action_parameters.h"

#include <utils/common/warnings.h>

#include <ui/graphics/items/resource/resource_widget.h>

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

QnActionParameters::QnActionParameters(const QVariantMap &arguments) {
    setArguments(arguments);
    setItems(QVariant::fromValue<QnResourceList>(QnResourceList()));
}

QnActionParameters::QnActionParameters(const QVariant &items, const QVariantMap &arguments) {
    setArguments(arguments);
    setItems(items);
}

QnActionParameters::QnActionParameters(const QList<QGraphicsItem *> &items, const QVariantMap &arguments) {
    setArguments(arguments);
    setItems(QVariant::fromValue<QnResourceWidgetList>(QnActionParameterTypes::widgets(items)));
}

QnActionParameters::QnActionParameters(QnResourceWidget *widget, const QVariantMap &arguments) {
    setArguments(arguments);
    setItems(QVariant::fromValue<QnResourceWidget *>(widget));
}

QnActionParameters::QnActionParameters(const QnResourceWidgetList &widgets, const QVariantMap &arguments) {
    setArguments(arguments);
    setItems(QVariant::fromValue<QnResourceWidgetList>(widgets));
}

QnActionParameters::QnActionParameters(const QnWorkbenchLayoutList &layouts, const QVariantMap &arguments) {
    setArguments(arguments);
    setItems(QVariant::fromValue<QnWorkbenchLayoutList>(layouts));
}

QnActionParameters::QnActionParameters(const QnLayoutItemIndexList &layoutItems, const QVariantMap &arguments) {
    setArguments(arguments);
    setItems(QVariant::fromValue<QnLayoutItemIndexList>(layoutItems));
}

void QnActionParameters::setArguments(const QVariantMap &arguments) {
    for(QVariantMap::const_iterator pos = arguments.begin(); pos != arguments.end(); pos++)
        setArgument(pos.key(), pos.value());
}

void QnActionParameters::setArgument(const QString &key, const QVariant &value) {
    if(key.isEmpty() && !checkType(value)) 
        return;
    
    m_arguments[key] = value;
}

void QnActionParameters::setItems(const QVariant &items) {
    setArgument(QString(), items);
}

Qn::ActionParameterType QnActionParameters::type(const QString &key) const {
    return QnActionParameterTypes::type(argument(key));
}

int QnActionParameters::size(const QString &key) const {
    Q_UNUSED(key)
    return QnActionParameterTypes::size(items());
}

QnResourceList QnActionParameters::resources(const QString &key) const {
    return QnActionParameterTypes::resources(argument(key));
}

QnResourcePtr QnActionParameters::resource(const QString &key) const {
    Q_UNUSED(key)
    QnResourceList resources = this->resources();

    if(resources.size() != 1)
        qnWarning("Invalid number of target resources: expected %2, got %3.", 1, resources.size());

    return resources.isEmpty() ? QnResourcePtr() : resources.front();
}

QnLayoutItemIndexList QnActionParameters::layoutItems(const QString &key) const {
    return QnActionParameterTypes::layoutItems(argument(key));
}

QnWorkbenchLayoutList QnActionParameters::layouts(const QString &key) const {
    return QnActionParameterTypes::layouts(argument(key));
}

QnResourceWidget *QnActionParameters::widget(const QString &key) const {
    Q_UNUSED(key)
    QnResourceWidgetList widgets = this->widgets();

    if(widgets.size() != 1)
        qnWarning("Invalid number of target widgets: expected %2, got %3.", 1, widgets.size());

    return widgets.isEmpty() ? NULL : widgets.front();
}

QnResourceWidgetList QnActionParameters::widgets(const QString &key) const {
    return QnActionParameterTypes::widgets(argument(key));
}



