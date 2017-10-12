#pragma once

#include <QtQuickWidgets/QQuickWidget>

#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/raii_guard.h>

#include <utils/common/connective.h>
#include <utils/common/encoded_credentials.h>

class QnWorkbenchWelcomeScreen: public Connective<QQuickWidget>, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QQuickWidget> base_type;

    Q_PROPERTY(QString cloudUserName READ cloudUserName NOTIFY cloudUserNameChanged)
    Q_PROPERTY(bool isLoggedInToCloud READ isLoggedInToCloud NOTIFY isLoggedInToCloudChanged)
    Q_PROPERTY(bool isCloudEnabled READ isCloudEnabled NOTIFY isCloudEnabledChanged)

    Q_PROPERTY(bool visibleControls READ visibleControls WRITE setVisibleControls NOTIFY visibleControlsChanged)
    Q_PROPERTY(QString connectingToSystem READ connectingToSystem WRITE setConnectingToSystem NOTIFY connectingToSystemChanged)
    Q_PROPERTY(bool globalPreloaderVisible READ globalPreloaderVisible WRITE setGlobalPreloaderVisible NOTIFY globalPreloaderVisibleChanged)

    Q_PROPERTY(QString minSupportedVersion READ minSupportedVersion CONSTANT)

    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged)

public:
    QnWorkbenchWelcomeScreen(QQmlEngine* engine, QWidget* parent = nullptr);

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

    void connectToCloudSystem(const QString& systemId, const QString& serverUrl);

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
        const QUrl& serverUrl,
        const QnEncodedCredentials& credentials,
        bool storePassword,
        bool autoLogin,
        const QnRaiiGuardPtr& completionTracker = QnRaiiGuardPtr());

    void handleStartupTileAction(const QString& systemId, bool initial);

protected:
    virtual void showEvent(QShowEvent* event) override;
    virtual void hideEvent(QHideEvent* event) override;

private:
    bool m_receivingResources = false;
    bool m_visibleControls = false;
    QString m_connectingSystemName;
    QString m_message;
};
