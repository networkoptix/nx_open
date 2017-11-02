#include "mobile_client_message_processor.h"

#include <core/resource/resource.h>
#include <core/resource/mobile_client_camera_factory.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <nx/network/socket_common.h>
#include <compatibility/user_permissions.h>

namespace detail {

const QnSoftwareVersion kUserPermissionsRefactoredVersion(3, 0);

} using namespace detail;

QnMobileClientMessageProcessor::QnMobileClientMessageProcessor(QObject* parent):
    base_type(parent)
{
}

bool QnMobileClientMessageProcessor::isConnected() const
{
    switch (connectionStatus()->state())
    {
        case QnConnectionState::Connected:
        case QnConnectionState::Reconnecting:
        case QnConnectionState::Ready:
            return true;
        default:
            return false;
    }
}

void QnMobileClientMessageProcessor::updateResource(
    const QnResourcePtr &resource,
    ec2::NotificationSource source)
{
    using namespace nx::common::compatibility::user_permissions;

    // TODO: #mshevchenko #3.1 Refactor it to use API versioning instead.
    const auto& info = commonModule()->ec2Connection()->connectionInfo();
    if (info.version < kUserPermissionsRefactoredVersion)
    {
        if (const auto user = resource.dynamicCast<QnUserResource>())
        {
            const auto v26Permissions =
                static_cast<GlobalPermissionsV26>(static_cast<int>(user->getRawPermissions()));
            user->setRawPermissions(migrateFromV26(v26Permissions));
        }
    }

    base_type::updateResource(resource, source);

    if (resource->getId() == commonModule()->remoteGUID())
        updateMainServerApiUrl(resource.dynamicCast<QnMediaServerResource>());
}

QnResourceFactory* QnMobileClientMessageProcessor::getResourceFactory() const
{
    return QnMobileClientCameraFactory::instance();
}

void QnMobileClientMessageProcessor::updateMainServerApiUrl(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return;

    nx::utils::Url url = commonModule()->currentUrl();
    if (!url.isValid())
        return;

    server->setPrimaryAddress(SocketAddress(url.host(), url.port(0)));
}
