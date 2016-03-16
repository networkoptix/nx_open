
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <ui/workbench/workbench_context_aware.h>

class QQuickView;
class QnLoginDialog;
class QnCloudStatusWatcher;

class QnWorkbenchWelcomeScreen : public Connective<QObject>
    , public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;
    
    Q_PROPERTY(bool isVisible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool isEnabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged);
    
    Q_PROPERTY(QString cloudUserName READ cloudUserName NOTIFY cloudUserNameChanged);
    Q_PROPERTY(bool isLoggedInToCloud READ isLoggedInToCloud NOTIFY isLoggedInToCloudChanged)

public:
    QnWorkbenchWelcomeScreen(QObject *parent);

    virtual ~QnWorkbenchWelcomeScreen();
    
    QWidget *widget();

public: // Properties
    bool isVisible() const;

    void setVisible(bool isVisible);

    bool isEnabled() const;

    void setEnabled(bool isEnabled);

    QString cloudUserName() const;

    bool isLoggedInToCloud() const;

public slots:
    void connectToLocalSystem(const QString &serverUrl
        , const QString &userName
        , const QString &password);

    void connectToCloudSystem(const QString &serverUrl);

    void connectToAnotherSystem();

    void logoutFromCloud();

    void manageCloudAccount();

    void loginToCloud();

    void createAccount();

    void tryHideScreen();

signals:
    void visibleChanged();
    
    void enabledChanged();

    void cloudUserNameChanged();

    void isLoggedInToCloudChanged();

private:
    void showScreen();

    void enableScreen();


private:
    typedef QPointer<QWidget> WidgetPtr;
    typedef QPointer<QnLoginDialog> LoginDialogPtr;
    typedef QPointer<QnCloudStatusWatcher> CloudStatusWatcherPtr;
    
    const CloudStatusWatcherPtr m_cloudWatcher;

    const WidgetPtr m_widget;
    
    LoginDialogPtr m_loginDialog;

    bool m_visible;
    bool m_enabled;
};