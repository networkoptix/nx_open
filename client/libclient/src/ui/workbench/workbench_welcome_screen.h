
#pragma once

#include <QtCore/QUrl>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/style/generic_palette.h>

class QnCloudStatusWatcher;
typedef QList<QUrl> UrlsList;

class QnWorkbenchWelcomeScreen : public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

    Q_PROPERTY(bool isVisible READ isVisible WRITE setVisible NOTIFY visibleChanged)

    Q_PROPERTY(QString cloudUserName READ cloudUserName NOTIFY cloudUserNameChanged);
    Q_PROPERTY(bool isLoggedInToCloud READ isLoggedInToCloud NOTIFY isLoggedInToCloudChanged)
    Q_PROPERTY(bool isOfflineConnection READ isOfflineConnection NOTIFY isOfflineConnectionChanged)

    Q_PROPERTY(QSize pageSize READ pageSize WRITE setPageSize NOTIFY pageSizeChanged);
    Q_PROPERTY(bool visibleControls READ visibleControls WRITE setVisibleControls NOTIFY visibleControlsChanged)
    Q_PROPERTY(QString connectingToSystem READ connectingToSystem WRITE setConnectingToSystem NOTIFY connectingToSystemChanged)
    Q_PROPERTY(bool globalPreloaderVisible READ globalPreloaderVisible WRITE setGlobalPreloaderVisible NOTIFY globalPreloaderVisibleChanged)

    Q_PROPERTY(QString softwareVersion READ softwareVersion CONSTANT)
    Q_PROPERTY(QString minSupportedVersion READ minSupportedVersion CONSTANT)

public:
    QnWorkbenchWelcomeScreen(QObject* parent);

    virtual ~QnWorkbenchWelcomeScreen();

    QWidget* widget();

public: // Properties
    bool isVisible() const;

    void setVisible(bool isVisible);

    QString cloudUserName() const;

    bool isLoggedInToCloud() const;

    bool isOfflineConnection() const;

    QSize pageSize() const;

    void setPageSize(const QSize& size);

    bool visibleControls() const;

    void setVisibleControls(bool visible);

    QString connectingToSystem() const;

    void setConnectingToSystem(const QString& value);

    bool globalPreloaderVisible() const;

    void setGlobalPreloaderVisible(bool value);

    QString softwareVersion() const;

    QString minSupportedVersion() const;

public slots:
    bool isAcceptableDrag(const UrlsList& urls);

    void makeDrop(const UrlsList& urls);

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

    void setupFactorySystem(const QString& serverUrl);

    void logoutFromCloud();

    void manageCloudAccount();

    void loginToCloud();

    void createAccount();

public slots:
    QColor getPaletteColor(const QString& group, int index);

    QColor getDarkerColor(const QColor& color, int offset = 1);

    QColor getLighterColor(const QColor& color, int offset = 1);

    QColor colorWithAlpha(QColor color, qreal alpha);

signals:
    void visibleChanged();

    void cloudUserNameChanged();

    void isLoggedInToCloudChanged();

    void isOfflineConnectionChanged();

    void pageSizeChanged();

    void visibleControlsChanged();

    void connectingToSystemChanged();

    void resetAutoLogin();

    void globalPreloaderVisibleChanged();

private:
    void showScreen();

    void enableScreen();

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
    const WidgetPtr m_widget;
    QSize m_pageSize;
};
