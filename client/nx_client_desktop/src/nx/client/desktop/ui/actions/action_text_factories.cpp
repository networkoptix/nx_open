#include "action_text_factories.h"

#include <ui/actions/action_parameter_types.h>
#include <ui/actions/action_parameters.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_item_index.h>

#include <utils/common/warnings.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

TextFactory::TextFactory(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
}

QString TextFactory::text(const QnActionParameters& /*parameters*/) const
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

QString DevicesNameTextFactory::text(const QnActionParameters& parameters) const
{
    const auto resources = parameters.resources();
    return QnDeviceDependentStrings::getNameFromSet(
        resourcePool(),
        m_stringSet,
        resources.filtered<QnVirtualCameraResource>());
}

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
