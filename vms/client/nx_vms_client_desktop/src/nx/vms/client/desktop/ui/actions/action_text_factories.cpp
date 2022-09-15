// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_text_factories.h"

#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {
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

FunctionalTextFactory::FunctionalTextFactory(TextFunction&& textFunction, QObject* parent):
    TextFactory(parent),
    m_textFunction(std::move(textFunction))
{
}

QString FunctionalTextFactory::text(
    const Parameters& parameters, QnWorkbenchContext* context) const
{
    return m_textFunction(parameters, context);
}

} // namespace action
} // namespace ui
} // namespace nx::vms::client::desktop
