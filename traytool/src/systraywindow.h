#ifndef WINDOW_H
#define WINDOW_H

#include <QSystemTrayIcon>
#include <QDialog>
#include <QString>

class QAction;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QSpinBox;
class QTextEdit;

namespace Ui {
    class SettingsDialog;
}

bool MyIsUserAnAdmin();

class QnElevationChecker : public QObject
{
    Q_OBJECT

public:
    QnElevationChecker(QObject* parent, QString actionName, QObject* target, const char* slot);

signals:
    void elevationCheckPassed();

private slots:
    void triggered();

private:
    QString m_actionName;
    QObject* m_target;
    const char* m_slot;
    bool m_rightWarnShowed;
};

class QnSystrayWindow : public QDialog
{
    Q_OBJECT

public:
    QnSystrayWindow();
    virtual ~QnSystrayWindow();
    QSystemTrayIcon* getTrayIcon() const { return trayIcon; }
    void executeAction(QString cmd);
    void setVisible(bool visible);

protected:
    virtual void closeEvent(QCloseEvent *event) override;

private slots:
    void handleMessage(const QString& message);
    
    void onDelayedMessage();

    void setIcon(int index);
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void showMessage(const QString& message);
    void messageClicked();

    void at_mediaServerStartAction();
    void at_mediaServerStopAction();
    void at_appServerStartAction();
    void at_appServerStopAction();

    void findServiceInfo();
    void updateServiceInfo();

    void buttonClicked(QAbstractButton * button);
    void onSettingsAction();
    void onShowMediaServerLogAction();
    void onShowAppServerLogAction();

    void onTestButtonClicked();
private:
    QAction* actionByName(const QString& name);
    QString nameByAction(QAction* action);

    void connectElevatedAction(QAction* source, const char* signal, QObject* target, const char* slot);

    void createActions();
    void createTrayIcon();

    int updateServiceInfoInternal(SC_HANDLE service, const QString& serviceName, QAction* startAction, QAction* stopAction, QAction* logAction);
    bool validateData();
    void saveData();
    bool checkPortNum(int port, const QString& message);
    bool isPortFree(int port);
    QUrl getAppServerURL() const;
    void setAppServerURL(const QUrl& url);
    bool isAppServerParamChanged() const;
    bool isMediaServerParamChanged() const;

    QScopedPointer<Ui::SettingsDialog> ui;

    QSettings m_settings;
    QSettings m_mServerSettings;
    QSettings m_appServerSettings;

    QAction *m_showMediaServerLogAction;
    QAction *m_showAppLogAction;
    QAction *settingsAction;
    QAction *quitAction;

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;

    QString m_detailedErrorText;
    QIcon m_iconOK;
    QIcon m_iconBad;

    SC_HANDLE m_scManager;
    SC_HANDLE m_mediaServerHandle;
    SC_HANDLE m_appServerHandle;
    
    QAction* m_mediaServerStartAction;
    QAction* m_mediaServerStopAction;
    QAction* m_appServerStartAction;
    QAction* m_appServerStopAction;
    bool m_firstTimeToolTipError;
    QTimer m_findServices;
    QTimer m_updateServiceStatus;

    bool m_needStartMediaServer;
    bool m_needStartAppServer;
    int m_skipTicks;

    typedef QPair<QString, QAction*> NameAndAction;
    typedef QList<NameAndAction> NameActionList;

    int m_prevMediaServerStatus;
    int m_prevAppServerStatus;

    NameActionList m_actionList;
    QTime m_lastMessageTimer;
    QQueue<QString> m_delayedMessages;
};

#endif
