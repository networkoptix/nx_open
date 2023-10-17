// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_text_factories.h"

#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>

#include "action_parameters.h"

namespace nx::vms::client::desktop {
namespace menu {

TextFactory::TextFactory(QObject* parent):
    QObject(parent)
{
}

QString TextFactory::text(
    const Parameters& /*parameters*/,
    WindowContext* /*context*/) const
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

QString DevicesNameTextFactory::text(
    const Parameters& parameters,
    WindowContext* context) const
{
    const auto resources = parameters.resources();
    return QnDeviceDependentStrings::getNameFromSet(
        context->system()->resourcePool(),
        m_stringSet,
        resources.filtered<QnVirtualCameraResource>());
}

FunctionalTextFactory::FunctionalTextFactory(TextFunction&& textFunction, QObject* parent):
    TextFactory(parent),
    m_textFunction(std::move(textFunction))
{
}

QString FunctionalTextFactory::text(
    const Parameters& parameters,
    WindowContext* context) const
{
    return m_textFunction(parameters, context);
}

} // namespace menu
} // namespace nx::vms::client::desktop
