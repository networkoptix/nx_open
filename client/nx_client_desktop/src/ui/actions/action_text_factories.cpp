#include "action_text_factories.h"

#include <ui/actions/action_parameter_types.h>
#include <ui/actions/action_parameters.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_item_index.h>

#include <utils/common/warnings.h>

using namespace nx::client::desktop::ui::action;

QnActionTextFactory::QnActionTextFactory(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
}

QString QnActionTextFactory::text(const QnResourceList &) const
{
    return QString();
}

QString QnActionTextFactory::text(const QnLayoutItemIndexList &layoutItems) const
{
    return text(QnActionParameterTypes::resources(layoutItems));
}

QString QnActionTextFactory::text(const QnResourceWidgetList &widgets) const
{
    return text(QnActionParameterTypes::layoutItems(widgets));
}

QString QnActionTextFactory::text(const QnWorkbenchLayoutList &layouts) const
{
    return text(QnActionParameterTypes::resources(layouts));
}

QString QnActionTextFactory::text(const QnActionParameters &parameters) const
{
    switch (parameters.type())
    {
        case ResourceType:
            return text(parameters.resources());
        case WidgetType:
            return text(parameters.widgets());
        case LayoutType:
            return text(parameters.layouts());
        case LayoutItemType:
            return text(parameters.layoutItems());
        default:
            qnWarning("Invalid action condition parameter type '%1'.", parameters.items().typeName());
            return QString();
    }
}

QString QnDevicesNameActionTextFactory::text(const QnResourceList &resources) const
{
    return QnDeviceDependentStrings::getNameFromSet(
        resourcePool(),
        m_stringSet,
        resources.filtered<QnVirtualCameraResource>());
}
