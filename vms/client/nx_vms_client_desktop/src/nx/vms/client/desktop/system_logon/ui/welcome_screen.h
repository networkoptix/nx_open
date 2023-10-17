// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtQuickWidgets/QQuickWidget>

#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/core/network/remote_connection_fwd.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::common { struct Credentials; }
namespace nx::utils { class Url; }
namespace nx::network::http { class AsyncHttpClientPtr; }

class QnUuid;

namespace nx::vms::client::desktop {

class ConnectTilesProxyModel;

class WelcomeScreen: public QQuickWidget, public WindowContextAware
{
    Q_OBJECT
    using base_type = QQuickWidget;

    Q_PROPERTY(QObject* systemModel READ systemModel NOTIFY systemModelChanged)

    Q_PROPERTY(bool hasCloudConnectionIssue
        READ hasCloudConnectionIssue
        NOTIFY hasCloudConnectionIssueChanged)

    Q_PROPERTY(QString cloudUserName READ cloudUserName NOTIFY cloudUserNameChanged)
    Q_PROPERTY(bool isCloudEnabled READ isCloudEnabled NOTIFY isCloudEnabledChanged)
    Q_PROPERTY(bool saveCredentialsAllowed READ saveCredentialsAllowed CONSTANT)
    Q_PROPERTY(bool is2FaEnabledForUser READ is2FaEnabledForUser NOTIFY is2FaEnabledForUserChanged)

    Q_PROPERTY(bool visibleControls
        READ visibleControls
        WRITE setVisibleControls
        NOTIFY visibleControlsChanged)

    Q_PROPERTY(bool gridEnabled READ gridEnabled NOTIFY gridEnabledChanged)

    Q_PROPERTY(bool globalPreloaderVisible
        READ globalPreloaderVisible
        WRITE setGlobalPreloaderVisible
        NOTIFY globalPreloaderVisibleChanged)

    Q_PROPERTY(bool globalPreloaderEnabled
        READ globalPreloaderEnabled
        WRITE setGlobalPreloaderEnabled
        NOTIFY globalPreloaderEnabledChanged)

    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged)

    Q_PROPERTY(int simpleModeTilesNumber READ simpleModeTilesNumber CONSTANT)
    Q_PROPERTY(bool richTextEnabled READ richTextEnabled CONSTANT)

public:
    WelcomeScreen(WindowContext* context, QWidget* parent = nullptr);
    virtual ~WelcomeScreen() override;

    void connectionToSystemEstablished(const QnUuid& systemId);

public: // Properties
    QObject* systemModel();

    bool hasCloudConnectionIssue() const;

    QString cloudUserName() const;

    bool isCloudEnabled() const;

    bool visibleControls() const;

    void setVisibleControls(bool visible);

    bool gridEnabled() const;

    void openConnectingTile(std::optional<core::RemoteConnectionErrorCode> errorCode);

    /** Check if there is an opened tile for the system, we are connecting to. */
    bool connectingTileExists() const;
    void setConnectingToSystem(const QString& value);

    bool globalPreloaderVisible() const;

    void setGlobalPreloaderVisible(bool value);

    bool globalPreloaderEnabled() const;

    Q_INVOKABLE void setGlobalPreloaderEnabled(bool value);

    void setMessage(const QString& message);

    QString message() const;

    int simpleModeTilesNumber() const;

    bool richTextEnabled() const;

    Q_INVOKABLE bool confirmCloudTileHiding() const;

    Q_INVOKABLE void openHelp() const;

    Q_INVOKABLE QString minSupportedVersion() const;

    Q_INVOKABLE void showSystemWentToOfflineNotification() const;

    Q_INVOKABLE bool checkUrlIsValid(const QString& urlText) const;

    Q_INVOKABLE void abortConnectionProcess();

public:
    void setupFactorySystem(const nx::network::SocketAddress& address, const QnUuid& serverId);

public slots:
    /** QML overload. */
    void setupFactorySystem(
        const QString& systemId,
        const QString& serverUrl,
        const QString& serverId);

    // TODO: #ynikitenkov Add multiple urls one-by-one handling.
    void connectToLocalSystem(
        const QString& systemId,
        const QString& serverId,
        const QString& serverUrl,
        const QString& userName,
        const QString& password,
        bool storePassword);

    // TODO: #ynikitenkov Add multiple urls one-by-one handling.
    void connectToLocalSystemUsingSavedPassword(
        const QString& systemId,
        const QString& serverId,
        const QString& serverUrl,
        const QString& userName,
        bool storePassword);

    void connectToCloudSystem(const QString& systemId);

    void connectToAnotherSystem();

    void testSystemOnline(const QString& serverUrl);

    void logoutFromCloud();

    void manageCloudAccount();

    void loginToCloud();

    void createAccount();

    void forceActiveFocus();

    void forgetUserPassword(const QString& localSystemId, const QString& user);

    void forgetAllCredentials(const QString& localSystemId);

    void deleteSystem(const QString& systemId, const QString& localSystemId);

signals:
    void systemModelChanged();

    void hasCloudConnectionIssueChanged();

    void cloudUserNameChanged();

    void is2FaEnabledForUserChanged();

    void isCloudEnabledChanged();

    void visibleControlsChanged();

    void gridEnabledChanged();

    void resetAutoLogin();

    void globalPreloaderVisibleChanged(bool visible);

    void globalPreloaderEnabledChanged();

    void messageChanged();

    void openTile(const QString& systemId, const QString& errorMessage = "",
        bool isLoginError = false);

    void closeTile(const QString& systemId);

    void switchPage(int pageIndex);

    void dropConnectingState() const;

private:
    bool isLoggedInToCloud() const;
    void createSystemModel();
    void connectToSystemInternal(
        const QString& systemId,
        const std::optional<QnUuid>& serverId,
        const nx::network::SocketAddress& address,
        const nx::network::http::Credentials& credentials,
        bool storePassword,
        const nx::utils::SharedGuardPtr& completionTracker = nullptr);

    bool saveCredentialsAllowed() const;
    bool is2FaEnabledForUser() const;

protected:
    virtual void showEvent(QShowEvent* event) override;
    virtual void hideEvent(QHideEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void changeEvent(QEvent* event) override;

private:
    bool m_globalPreloaderVisible = false;
    bool m_globalPreloaderEnabled = true;
    bool m_visibleControls = true;
    QString m_connectingSystemId;
    QString m_message;
    nx::vms::client::desktop::ConnectTilesProxyModel* m_systemModel = nullptr;

    QList<nx::network::http::AsyncHttpClientPtr> m_runningRequests;
};

} // namespace nx::vms::client::desktop
