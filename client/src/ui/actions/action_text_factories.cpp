#include "action_text_factories.h"

#include <core/resource/resource_name.h>

QnActionTextFactory::QnActionTextFactory(QObject *parent): 
    QObject(parent),
    QnWorkbenchContextAware(parent)
{}

Qn::ActionVisibility QnActionTextFactory::text(const QnResourceList &) { 
    return Qn::InvisibleAction; 
}

Qn::ActionVisibility QnActionTextFactory::text(const QnLayoutItemIndexList &layoutItems) { 
    return text(QnActionParameterTypes::resources(layoutItems));
}

Qn::ActionVisibility QnActionTextFactory::text(const QnResourceWidgetList &widgets) { 
    return text(QnActionParameterTypes::layoutItems(widgets));
}

Qn::ActionVisibility QnActionTextFactory::text(const QnWorkbenchLayoutList &layouts) { 
    return text(QnActionParameterTypes::resources(layouts));
}

Qn::ActionVisibility QnActionTextFactory::text(const QnActionParameters &parameters) {
    switch(parameters.type()) {
    case Qn::ResourceType:
        return text(parameters.resources());
    case Qn::WidgetType:
        return text(parameters.widgets());
    case Qn::LayoutType:
        return text(parameters.layouts());
    case Qn::LayoutItemType:
        return text(parameters.layoutItems());
    default:
        qnWarning("Invalid action condition parameter type '%1'.", parameters.items().typeName());
        return QString();
    }
}

QString QnDevicesNameActionTextFactory::text(const QnResourceList &resources) {
    return m_template.arg(getDevicesName(resources.filtered<QnVirtualCameraResource>()));
}
