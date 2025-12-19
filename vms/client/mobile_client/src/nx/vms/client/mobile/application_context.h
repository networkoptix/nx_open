// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/application_context.h>

Q_MOC_INCLUDE("nx/vms/client/mobile/push_notification/push_notification_manager.h")
Q_MOC_INCLUDE("nx/vms/client/mobile/ui/detail/credentials_helper.h")
Q_MOC_INCLUDE("settings/qml_settings_adaptor.h")
Q_MOC_INCLUDE("utils/mobile_app_info.h")

class QnCameraThumbnailProvider;
class QnMobileAppInfo;
class QnMobileClientSettings;
class QnResourceFactory;
struct QnMobileClientStartupParameters;

namespace nx::client::mobile { class QmlSettingsAdaptor; }

namespace nx::vms::client::mobile {

namespace detail { class CredentialsHelper; }

class AbstractSecureStorage;
class LocalSettings;
class PushNotificationManager;
class PushNotificationStorage;
class SystemContext;
class WindowContext;

class ApplicationContext: public core::ApplicationContext
{
    Q_OBJECT
    using base_type = core::ApplicationContext;

    Q_PROPERTY(QnMobileAppInfo* appInfo
        READ appInfo
        CONSTANT)

    Q_PROPERTY(PushNotificationManager* pushManager
        READ pushManager
        CONSTANT)

    Q_PROPERTY(detail::CredentialsHelper* credentialsHelper
        READ credentialsHelper
        CONSTANT)

    Q_PROPERTY(nx::client::mobile::QmlSettingsAdaptor* settings
        READ settings
        CONSTANT)

public:
    ApplicationContext(
        const QnMobileClientStartupParameters& startupParams,
        std::unique_ptr<QnMobileClientSettings> settings,
        QObject* parent = nullptr);
    virtual ~ApplicationContext() override;

    virtual core::SystemContext* createSystemContext(
        core::SystemContext::Mode mode, QObject* parent = nullptr) override;

    virtual nx::Uuid peerId() const override;

    SystemContext* currentSystemContext() const;

    WindowContext* mainWindowContext() const;

    Q_INVOKABLE void closeWindow();

    QnMobileAppInfo* appInfo() const;

    PushNotificationManager* pushManager() const;

    QnResourceFactory* resourceFactory() const;

    detail::CredentialsHelper* credentialsHelper() const;

    QnMobileClientSettings* clientSettings() const;
    nx::client::mobile::QmlSettingsAdaptor* settings() const;

    QnCameraThumbnailProvider* cameraThumbnailProvider() const;

    AbstractSecureStorage* secureStorage() const;

    PushNotificationStorage* pushNotificationStorage() const;

    LocalSettings* localSettings() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

inline ApplicationContext* appContext()
{
    return common::ApplicationContext::instance()->as<ApplicationContext>();
}

} // namespace nx::vms::client::mobile
