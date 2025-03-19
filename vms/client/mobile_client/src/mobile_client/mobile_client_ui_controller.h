// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>

Q_MOC_INCLUDE("core/resource/layout_resource.h")

class QnContext;
namespace nx::vms::client::mobile { class SessionManager; }

class QnMobileClientUiControllerPrivate;
class QnMobileClientUiController: public QObject
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
        BetaFeaturesScreen,
        DetailsScreen,
        EventSearchMenuScreen,
        EventSearchScreen,
        MenuScreen
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

//    Q_PROPERTY(QString currentSystemName
//        READ currentSystemName
//        NOTIFY currentSystemNameChanged)

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

    QnLayoutResource* rawLayout() const;
    void setRawLayout(QnLayoutResource* value);

//    QString currentSystemName() const;

public:
    Q_INVOKABLE void openConnectToServerScreen(
        const nx::utils::Url& url,
        const QString& operationId);
    Q_INVOKABLE void openResourcesScreen(const ResourceIdList& filterIds = ResourceIdList());
    Q_INVOKABLE void openVideoScreen(QnResource* cameraResource, qint64 timestamp);
    Q_INVOKABLE void openSessionsScreen();

signals:
    void layoutChanged();
    void resourcesScreenRequested(const QVariant& filterIds);
    void videoScreenRequested(QnResource* cameraResource, qint64 timestamp);
    void sessionsScreenRequested();

    void connectToServerScreenRequested(
        const QString& host,
        const QString& user,
        const QString& password,
        const QString& operationId);

//    void currentSystemNameChanged();

    void currentScreenChanged();

private:
    QScopedPointer<QnMobileClientUiControllerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMobileClientUiController)
};
