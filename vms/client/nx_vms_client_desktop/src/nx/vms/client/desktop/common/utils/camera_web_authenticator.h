// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QUrl>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

#include "abstract_web_authenticator.h"

namespace nx::vms::client::desktop {

/**
 * Provides camera web page authentication.
 */
class CameraWebAuthenticator:
    public AbstractWebAuthenticator,
    public SystemContextAware
{
    Q_OBJECT
public:
    CameraWebAuthenticator(
        QnVirtualCameraResourcePtr camera,
        QUrl settingsUrl,
        QString login,
        QString password);

    virtual void requestAuth(const QUrl& url) override;

private:
    QnVirtualCameraResourcePtr m_camera;
    QUrl m_settingsUrl;
    QString m_login;
    QString m_password;
};

} // namespace nx::vms::client::desktop
