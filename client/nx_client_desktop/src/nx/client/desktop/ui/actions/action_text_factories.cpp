#include "action_text_factories.h"

#include <nx/client/desktop/ui/actions/action_parameters.h>

#include <core/resource/camera_resource.h>

#include <ui/workbench/workbench_context.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

TextFactory::TextFactory(QObject* parent):
    QObject(parent)
{
}

QString TextFactory::text(const Parameters& /*parameters*/,
    QnWorkbenchContext* /*context*/) const
{
    return QString();
}

DevicesNameTextFactory::DevicesNameTextFactory(
    const QnCameraDeviceStringSet& stringSet,
    QObject* parent)
    :
    TextFactory(parent),
    m_stringSet(stringSet)
{

}

QString DevicesNameTextFactory::text(const Parameters& parameters,
    QnWorkbenchContext* context) const
{
    const auto resources = parameters.resources();
    return QnDeviceDependentStrings::getNameFromSet(
        context->resourcePool(),
        m_stringSet,
                resources.filtered<QnVirtualCameraResource>());
}

ConvertToEntropixTextFactory::ConvertToEntropixTextFactory(QObject* parent):
    TextFactory(parent)
{
}

QString ConvertToEntropixTextFactory::text(
    const Parameters& parameters,
    QnWorkbenchContext* /*context*/) const
{
    const auto& resouces = parameters.resources();

    bool entropix = std::all_of(resouces.begin(), resouces.end(),
        [](const QnResourcePtr& resource)
        {
            const auto& camera = resource.dynamicCast<QnVirtualCameraResource>();
            if (!camera)
                return false;

            return camera->hasCombinedSensors();
        });

    return entropix ? tr("Convert to Normal Camera") : tr("Convert to Entropix Camera");
}

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
