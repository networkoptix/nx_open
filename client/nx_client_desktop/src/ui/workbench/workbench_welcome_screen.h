
#pragma once

#include <QtCore/QUrl>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/style/generic_palette.h>

#include <nx/utils/raii_guard.h>

#include <utils/common/connective.h>
#include <utils/common/encoded_credentials.h>

class QnCloudStatusWatcher;
class QQuickView;
class QnAppInfo;

typedef QList<QUrl> UrlsList;

class QnWorkbenchWelcomeScreen : public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

    Q_PROPERTY(bool isVisible READ isVisible WRITE setVisible NOTIFY visibleChanged)

    Q_PROPERTY(QString cloudUserName READ cloudUserName NOTIFY cloudUserNameChanged)
    Q_PROPERTY(bool isLoggedInToCloud READ isLoggedInToCloud NOTIFY isLoggedInToCloudChanged)
    Q_PROPERTY(bool isCloudEnabled READ isCloudEnabled NOTIFY isCloudEnabledChanged)

    Q_PROPERTY(QSize pageSize READ pageSize WRITE setPageSize NOTIFY pageSizeChanged)
    Q_PROPERTY(bool visibleControls READ visibleControls WRITE setVisibleControls NOTIFY visibleControlsChanged)
    Q_PROPERTY(QString connectingToSystem READ connectingToSystem WRITE setConnectingToSystem NOTIFY connectingToSystemChanged)
    Q_PROPERTY(bool globalPreloaderVisible READ globalPreloaderVisible WRITE setGlobalPreloaderVisible NOTIFY globalPreloaderVisibleChanged)

    Q_PROPERTY(QString minSupportedVersion READ minSupportedVersion CONSTANT)

    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged);

    Q_PROPERTY(QnAppInfo* appInfo READ appInfo CONSTANT);
public:
    QnWorkbenchWelcomeScreen(QObject* parent);

    virtual ~QnWorkbenchWelcomeScreen();

    QWidget* widget();

public: // Properties
    bool isVisible() const;

    void setVisible(bool isVisible);

    QString cloudUserName() const;

    bool isLoggedInToCloud() const;

    bool isCloudEnabled() const;

    QSize pageSize() const;

    void setPageSize(const QSize& size);

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

    QnAppInfo* appInfo() const;

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
    QColor getContrastColor(const QString& group);

    QColor getPaletteColor(const QString& group, int index);

    QColor getDarkerColor(const QColor& color, int offset = 1);

    QColor getLighterColor(const QColor& color, int offset = 1);

    QColor colorWithAlpha(QColor color, qreal alpha);

    void hideSystem(const QString& systemId, const QString& localSystemId);

    void moveToBack(const QUuid& localSystemId);

signals:
    void visibleChanged();

    void cloudUserNameChanged();

    void isLoggedInToCloudChanged();

    void isCloudEnabledChanged();

    void pageSizeChanged();

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

private: // overrides
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    typedef QPointer<QWidget> WidgetPtr;
    typedef QPointer<QnCloudStatusWatcher> CloudStatusWatcherPtr;

    bool m_receivingResources;
    bool m_visibleControls;
    bool m_visible;
    QString m_connectingSystemName;
    const QnGenericPalette m_palette;
    QQuickView* m_quickView;
    const WidgetPtr m_widget;
    QSize m_pageSize;
    QString m_message;
    QnAppInfo* m_appInfo;
};
