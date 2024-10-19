// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/network/maintenance/log/uploader.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/singleton.h>
#include <nx_ec/ec_api_fwd.h>
#include <nx/vms/client/mobile/system_context_aware.h>

class QQuickWindow;

class QnContext;
class QnClientCoreModule;
struct QnMobileClientStartupParameters;
class QnAvailableCamerasWatcher;
class QnResourceDiscoveryManager;

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
    public Singleton<QnMobileClientModule>,
    public nx::vms::client::mobile::SystemContextAware
{
    Q_OBJECT

    using base_type = nx::vms::client::mobile::SystemContextAware;
public:
    QnMobileClientModule(
        nx::vms::client::mobile::SystemContext* context,
        const QnMobileClientStartupParameters& startupParameters,
        QObject* parent = nullptr);
    virtual ~QnMobileClientModule() override;

    QnContext* context() const;

    void initDesktopCamera();

    void startLocalSearches();

    nx::vms::client::mobile::PushNotificationManager* pushManager();

    QQuickWindow* mainWindow() const;
    void setMainWindow(QQuickWindow* window);

    nx::network::maintenance::log::UploaderManager* remoteLogsUploader();

    QnAvailableCamerasWatcher* availableCamerasWatcher() const;

    nx::vms::client::mobile::WebViewController* webViewController() const;

    QnResourceDiscoveryManager* resourceDiscoveryManager() const;

private:
    void initializeSessionTimeoutWatcher();
    void initializePushNotifications();
    void initializeConnectionUserInteractionDelegate();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;

    std::unique_ptr<QnClientCoreModule> m_clientCoreModule;
    QScopedPointer<nx::vms::client::mobile::PushNotificationManager> m_pushManager;
    QScopedPointer<QnContext> m_context;
    QQuickWindow* m_mainWindow = nullptr;
    std::unique_ptr<nx::network::maintenance::log::UploaderManager> m_logUploader;
    std::unique_ptr<nx::vms::client::mobile::RemoteLogManager> m_remoteLogManager;
};

#define qnMobileClientModule QnMobileClientModule::instance()
