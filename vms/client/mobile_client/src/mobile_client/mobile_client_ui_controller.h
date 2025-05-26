// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/mobile/window_context_aware.h>

Q_MOC_INCLUDE("core/resource/layout_resource.h")
Q_MOC_INCLUDE("nx/vms/client/mobile/utils/operation_manager.h")

namespace nx::vms::client::mobile {

class SessionManager;
class OperationManager;

} // namespace nx::vms::client::mobile

class QnMobileClientUiControllerPrivate;
class QnMobileClientUiController:
    public QObject,
    public nx::vms::client::mobile::WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum class Screen
    {
        UnknownScreen,
        SessionsScreen,
        CustomConnectionScreen,
        ResourcesScreen,
        VideoScreen,
        LoginToCloudScreen,
        DigestLoginToCloudScreen,
        CameraSettingsScreen,
        DetailsScreen,
        EventSearchMenuScreen,
        EventSearchScreen,
        MenuScreen,

        // Application settings related screens.
        SettingsScreen,
        SecuritySettingsScreen,
        InterfaceSettingsScreen,
        PerformanceSettingsScreen,
        BetaFeaturesScreen,
        PushExpertSettingsScreen,
        AppInfoScreen
    };

    Q_ENUM(Screen)

private:
    Q_PROPERTY(Screen currentScreen
        READ currentScreen
        WRITE setCurrentScreen
        NOTIFY currentScreenChanged)

    Q_PROPERTY(QnLayoutResource* layout
        READ rawLayout
        WRITE setRawLayout
        NOTIFY layoutChanged)

    Q_PROPERTY(nx::vms::client::mobile::OperationManager* operationManager
        READ operationManager
        CONSTANT)

    // Temporary solution to block connection errors and screen changed during connection,
    // until we refactor UI classes.
    Q_PROPERTY(bool avoidHandlingConnectionStuff
        READ avoidHandlingConnectionStuff
        WRITE setAvoidHandlingConnectionStuff
        NOTIFY avoidHandlingConnectionStuffChanged)

    using ResourceIdList = QList<nx::Uuid>;
    using SessionManager = nx::vms::client::mobile::SessionManager;

public:
    QnMobileClientUiController(
        nx::vms::client::mobile::WindowContext* context,
        QObject* parent = nullptr);

    ~QnMobileClientUiController();


public:
    // Properties section.
    Screen currentScreen() const;
    void setCurrentScreen(Screen value);

    QnLayoutResource* rawLayout() const;
    void setRawLayout(QnLayoutResource* value);

    bool avoidHandlingConnectionStuff() const;
    void setAvoidHandlingConnectionStuff(bool value);

    nx::vms::client::mobile::OperationManager* operationManager() const;

public:
    Q_INVOKABLE void openConnectToServerScreen(
        const nx::Url& url,
        const QString& operationId);
    Q_INVOKABLE void openResourcesScreen(const ResourceIdList& filterIds = ResourceIdList());
    Q_INVOKABLE void openVideoScreen(QnResource* cameraResource, qint64 timestamp);
    Q_INVOKABLE void openSessionsScreen();

signals:
    void layoutChanged();
    void resourcesScreenRequested(const QVariant& filterIds);
    void videoScreenRequested(QnResource* cameraResource, qint64 timestamp);
    void sessionsScreenRequested();
    void avoidHandlingConnectionStuffChanged();

    void connectToServerScreenRequested(
        const QString& host,
        const QString& user,
        const QString& password,
        const QString& operationId);

    void currentScreenChanged();

private:
    QScopedPointer<QnMobileClientUiControllerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMobileClientUiController)
};
