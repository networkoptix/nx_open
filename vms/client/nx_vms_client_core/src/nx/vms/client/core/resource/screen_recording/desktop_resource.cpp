// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_resource.h"

#include <core/resource/media_server_resource.h>
#include <nx/vms/common/application_context.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <utils/common/long_runable_cleanup.h>

#include "desktop_camera_connection.h"
#include "desktop_data_provider_base.h"

using namespace nx::vms::client::core;

namespace {

static const nx::Uuid kDesktopResourceUuid("{B3B2235F-D279-4d28-9012-00DE1002A61D}");

} // namespace

namespace nx::vms::client::core {

DesktopResource::DesktopResource()
{
    const auto kName = "Desktop";

    addFlags(Qn::local_live_cam | Qn::desktop_camera);
    setName(kName);
    setUrl(kName);
    setIdUnsafe(kDesktopResourceUuid);
}

DesktopResource::~DesktopResource()
{
    disconnectFromServer();
}

nx::Uuid DesktopResource::getDesktopResourceUuid()
{
    return kDesktopResourceUuid;
}

bool DesktopResource::isConnected() const
{
    return static_cast<bool>(m_connection);
}

void DesktopResource::initializeConnection(
    const QnMediaServerResourcePtr& server,
    const nx::Uuid& userId)
{
    NX_ASSERT(!m_connection, "Double initialization");
    disconnectFromServer();

    m_connection.reset(new DesktopCameraConnection(
        toSharedPointer(this),
        server,
        userId,
        systemContext()->peerId(),
        systemContext()->messageBusConnection()->credentials()));
    m_connection->start();
}

void DesktopResource::disconnectFromServer()
{
    if (!m_connection)
        return;

    common::appContext()->longRunableCleanup()->cleanupAsync(std::move(m_connection));
}

AudioLayoutConstPtr DesktopResource::getAudioLayout(
    const QnAbstractStreamDataProvider* dataProvider) const
{
    auto desktopProvider = dynamic_cast<DesktopDataProviderBase*>(
        const_cast<QnAbstractStreamDataProvider*>(dataProvider));
    if (desktopProvider)
        return desktopProvider->getAudioLayout();

    return nullptr;
}

QString DesktopResource::calculateUniqueId(const nx::Uuid& moduleId, const nx::Uuid& userId)
{
    return moduleId.toString() + userId.toString();
}

} // namespace nx::vms::client::core
