#pragma once

#include <memory>

#include <QtQuickWidgets/QQuickWidget>

#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/scope_guard.h>

#include <utils/common/connective.h>

namespace nx::vms::common { struct Credentials; }
namespace nx::utils { class Url; }

class QnStartupTileManager;

class QnWorkbenchWelcomeScreen: public Connective<QQuickWidget>, public QnWorkbenchContextAware
{

    Q_OBJECT
    typedef Connective<QQuickWidget> base_type;

    Q_PROPERTY(QString cloudUserName READ cloudUserName NOTIFY cloudUserNameChanged)
    Q_PROPERTY(bool isLoggedInToCloud READ isLoggedInToCloud NOTIFY isLoggedInToCloudChanged)
    Q_PROPERTY(bool isCloudEnabled READ isCloudEnabled NOTIFY isCloudEnabledChanged)
    Q_PROPERTY(bool saveCredentialsAllowed READ saveCredentialsAllowed CONSTANT)

    Q_PROPERTY(bool visibleControls READ visibleControls WRITE setVisibleControls NOTIFY visibleControlsChanged)
    Q_PROPERTY(QString connectingToSystem READ connectingToSystem WRITE setConnectingToSystem NOTIFY connectingToSystemChanged)
    Q_PROPERTY(bool globalPreloaderVisible READ globalPreloaderVisible WRITE setGlobalPreloaderVisible NOTIFY globalPreloaderVisibleChanged)

    Q_PROPERTY(QString minSupportedVersion READ minSupportedVersion CONSTANT)

    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged)

    Q_PROPERTY(int tileHideOptions READ tileHideOptions CONSTANT)

public:
    QnWorkbenchWelcomeScreen(QWidget* parent = nullptr);

    virtual ~QnWorkbenchWelcomeScreen() override;

public: // Properties
    QString cloudUserName() const;

    bool isLoggedInToCloud() const;

    bool isCloudEnabled() const;

    bool visibleControls() const;

    void setVisibleControls(bool visible);

    QString connectingToSystem() const;

    void openConnectingTile();

    void handleDisconnectedFromSystem();

    void handleConnectingToSystem();

    void setConnectingToSystem(const QString& value);

    bool globalPreloaderVisible() const;

    void setGlobalPreloaderVisible(bool value);

    QString minSupportedVersion() const;

    void setMessage(const QString& message);

    QString message() const;

    int tileHideOptions() const;

    /**
     * If user never connected or disconnected manually, and there is only one system, initialize
     * connection to this system automatically.
     */
    void initializeStartupTilesHandling();

    /**
     * Stop auto-connection to the first found system if user explicitly did something.
     */
    void skipStartupTilesHandling();

public:
    void setupFactorySystem(const QString& serverUrl);

public slots:
    bool isAcceptableDrag(const QList<QUrl>& urls);

    void makeDrop(const QList<QUrl>& urls);

    // TODO: $ynikitenkov add multiple urls one-by-one  handling
    void connectToLocalSystem(
        const QString& systemId,
        const QString& serverUrl,
        const QString& userName,
        const QString& password,
        bool storePassword,
        bool autoLogin);

    void connectToCloudSystem(const QString& systemId);

    void connectToAnotherSystem();

    void logoutFromCloud();

    void manageCloudAccount();

    void loginToCloud();

    void createAccount();

    void forceActiveFocus();

    void forgetPassword(
        const QString& localSystemId,
        const QString& userName);

public slots:
    void hideSystem(const QString& systemId, const QString& localSystemId);

signals:
    void cloudUserNameChanged();

    void isLoggedInToCloudChanged();

    void isCloudEnabledChanged();

    void visibleControlsChanged();

    void connectingToSystemChanged();

    void resetAutoLogin();

    void globalPreloaderVisibleChanged();

    void messageChanged();

    void openTile(const QString& systemId);

    void switchPage(int pageIndex);

private:
    void connectToSystemInternal(
        const QString& systemId,
        const nx::utils::Url& serverUrl,
        const nx::vms::common::Credentials& credentials,
        bool storePassword,
        bool autoLogin,
        const nx::utils::SharedGuardPtr& completionTracker = nullptr);

    void handleStartupTileAction(const QString& systemId, bool initial);

    bool saveCredentialsAllowed() const;

protected:
    virtual void showEvent(QShowEvent* event) override;
    virtual void hideEvent(QHideEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void changeEvent(QEvent* event) override;

private:
    bool m_receivingResources = false;
    bool m_visibleControls = false;
    QString m_connectingSystemName;
    QString m_message;
    std::unique_ptr<QnStartupTileManager> m_startupTileManager;
};
