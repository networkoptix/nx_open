// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_resource_base.h"

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <utils/common/long_runable_cleanup.h>

#include "desktop_camera_connection.h"
#include "desktop_data_provider_base.h"

namespace {

static const QnUuid kDesktopResourceUuid("{B3B2235F-D279-4d28-9012-00DE1002A61D}");

} // namespace

QnDesktopResource::QnDesktopResource()
{
    const auto kName = "Desktop";

    addFlags(Qn::local_live_cam | Qn::desktop_camera);
    setName(kName);
    setUrl(kName);
    setIdUnsafe(kDesktopResourceUuid);
}

QnDesktopResource::~QnDesktopResource()
{
    disconnectFromServer();
}

QnUuid QnDesktopResource::getDesktopResourceUuid()
{
    return kDesktopResourceUuid;
}

bool QnDesktopResource::isConnected() const
{
    return static_cast<bool>(m_connection);
}

void QnDesktopResource::initializeConnection(
    const QnMediaServerResourcePtr& server,
    const QnUuid& userId)
{
    NX_ASSERT(!m_connection, "Double initialization");
    disconnectFromServer();

    nx::vms::client::core::RemoteConnectionAware credentialsAccessor;
    NX_ASSERT(credentialsAccessor.connection());

    m_connection.reset(new QnDesktopCameraConnection(
        toSharedPointer(this),
        server,
        userId,
        commonModule()->moduleGUID(),
        credentialsAccessor.connectionCredentials()));
    m_connection->start();
}

void QnDesktopResource::disconnectFromServer()
{
    if (!m_connection)
        return;

    if (auto cleanup = QnLongRunableCleanup::instance())
        cleanup->cleanupAsync(std::move(m_connection));
}

AudioLayoutConstPtr QnDesktopResource::getAudioLayout(
    const QnAbstractStreamDataProvider* dataProvider) const
{
    auto desktopProvider = dynamic_cast<QnDesktopDataProviderBase*>(
        const_cast<QnAbstractStreamDataProvider*>(dataProvider));
    if (desktopProvider)
        return desktopProvider->getAudioLayout();

    return nullptr;
}

QString QnDesktopResource::calculateUniqueId(const QnUuid& moduleId, const QnUuid& userId)
{
    return moduleId.toString() + userId.toString();
}
