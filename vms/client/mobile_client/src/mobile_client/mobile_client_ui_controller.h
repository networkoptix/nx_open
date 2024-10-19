// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/mobile/current_system_context_aware.h>

class QnContext;
namespace nx::vms::client::mobile { class SessionManager; }

class QnMobileClientUiControllerPrivate;
class QnMobileClientUiController:
    public QObject,
    public nx::vms::client::mobile::CurrentSystemContextAware
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
        SettingsScreen,
        PushExpertSettingsScreen,
        LoginToCloudScreen,
        DigestLoginToCloudScreen,
        CameraSettingsScreen,
        BetaFeaturesScreen
    };

    Q_ENUM(Screen)

private:
    Q_PROPERTY(Screen currentScreen
        READ currentScreen
        WRITE setCurrentScreen
        NOTIFY currentScreenChanged)

    Q_PROPERTY(QString layoutId
        READ layoutId
        WRITE setLayoutId
        NOTIFY layoutIdChanged)

    Q_PROPERTY(QString currentSystemName
        READ currentSystemName
        NOTIFY currentSystemNameChanged)

    using ResourceIdList = QList<nx::Uuid>;
    using SessionManager = nx::vms::client::mobile::SessionManager;

public:
    QnMobileClientUiController(QObject* parent = nullptr);

    ~QnMobileClientUiController();

    void initialize(
        SessionManager* sessionManager,
        QnContext* context);

public:
    // Properties section.
    Screen currentScreen() const;
    void setCurrentScreen(Screen value);

    QString layoutId() const;
    void setLayoutId(const QString& layoutId);

    QString currentSystemName() const;

public:
    Q_INVOKABLE void openConnectToServerScreen(
        const nx::utils::Url& url,
        const QString& operationId);
    Q_INVOKABLE void openResourcesScreen(const ResourceIdList& filterIds = ResourceIdList());
    Q_INVOKABLE void openVideoScreen(const QnResource* cameraResource, qint64 timestamp);
    Q_INVOKABLE void openSessionsScreen();

signals:
    void layoutIdChanged();
    void resourcesScreenRequested(const QVariant& filterIds);
    void videoScreenRequested(const QnResource* cameraResource, qint64 timestamp);
    void sessionsScreenRequested();

    void connectToServerScreenRequested(
        const QString& host,
        const QString& user,
        const QString& password,
        const QString& operationId);

    void currentSystemNameChanged();

    void currentScreenChanged();

private:
    QScopedPointer<QnMobileClientUiControllerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMobileClientUiController)
};
