// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_web_authenticator.h"

#include <QtCore/QThread>
#include <QtNetwork/QAuthenticator>
#include <QtWidgets/QApplication>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

CameraWebAuthenticator::CameraWebAuthenticator(
    QnVirtualCameraResourcePtr camera,
    QUrl settingsUrl,
    QString login,
    QString password)
    :
    SystemContextAware(camera->systemContext()),
    m_camera(camera),
    m_settingsUrl(std::move(settingsUrl)),
    m_login(std::move(login)),
    m_password(std::move(password))
{
}

void CameraWebAuthenticator::requestAuth(const QUrl& url)
{
    NX_ASSERT(QThread::currentThread() == qApp->thread());

    // Camera may redirect to another path and ask for credentials, so check for the host match.
    if (m_settingsUrl.host() != url.host())
    {
        emit authReady({});
        return;
    }

    if (m_camera->getStatus() == nx::vms::api::ResourceStatus::unauthorized)
    {
        emit authReady({});
        return;
    }

    const auto resultCallback =
        [this](bool success, rest::Handle /*handle*/, const QAuthenticator& result)
        {
            if (success)
            {
                emit authReady(result);
                return;
            }

            QAuthenticator loginOnly;
            loginOnly.setUser(m_login);
            emit authReady(loginOnly);
        };

    connectedServerApi()->getCameraCredentials(
        m_camera->getId(),
        nx::utils::guarded(this, resultCallback),
        QThread::currentThread());
}

} // namespace nx::vms::client::desktop
