// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/network/maintenance/log/uploader.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/singleton.h>
#include <nx_ec/ec_api_fwd.h>
#include <nx/vms/client/mobile/system_context_aware.h>

class QQuickWindow;

class QnClientCoreModule;
class QnResourceDiscoveryManager;
struct QnMobileClientStartupParameters;


namespace nx::vms::client::core { class SystemContext; }
namespace nx::vms::client::mobile {
class PushNotificationManager;
class RemoteLogManager;
class SystemContext;
class WebViewController;
} // namespace nx::vms::client::mobile

// TODO: #sivanov #ynikitenkov Rename this class to nx::vms::client::mobile::ApplicationContext,
// inherited from nx::vms::client::core::ApplicationContext.
class QnMobileClientModule:
    public QObject,
    public Singleton<QnMobileClientModule>
{
    Q_OBJECT

public:
    QnMobileClientModule(
        const QnMobileClientStartupParameters& startupParameters,
        QObject* parent = nullptr);
    virtual ~QnMobileClientModule() override;

    nx::vms::client::mobile::PushNotificationManager* pushManager();

    nx::network::maintenance::log::UploaderManager* remoteLogsUploader();

private:
    void initializePushNotifications();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;

    QScopedPointer<nx::vms::client::mobile::PushNotificationManager> m_pushManager;
    std::unique_ptr<nx::network::maintenance::log::UploaderManager> m_logUploader;
    std::unique_ptr<nx::vms::client::mobile::RemoteLogManager> m_remoteLogManager;
};

#define qnMobileClientModule QnMobileClientModule::instance()
