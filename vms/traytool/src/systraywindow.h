#pragma once

#include <QtWidgets/QDialog>
#include <QtCore/QString>
#include <QtCore/QStringListModel>
#include <QtCore/QRunnable>
#include <QtCore/QTimer>
#include <QtCore/QTime>
#include <QtCore/QQueue>
#include <QtCore/QSettings>
#include <QtWidgets/QSystemTrayIcon>

#include <nx/utils/impl_ptr.h>

class FoundEnterpriseControllersModel;

class QAbstractButton;
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

namespace Ui { class SettingsDialog; }

class RunnableTask: public QObject, public QRunnable
{
    Q_OBJECT

signals:
    void finished(quint64);

};

class QnElevationChecker: public QObject
{
    Q_OBJECT
public:
    explicit QnElevationChecker(
        const QString& actionId,
        QObject* parent = nullptr);

    static bool isUserAnAdmin();

    void trigger();

signals:
    void elevationCheckPassed();

private:
    const QString m_actionId;
    bool m_errorDisplayedOnce = false;
};

class QnSystrayWindow: public QDialog
{
    Q_OBJECT
    typedef QDialog base_type;

public:
    QnSystrayWindow();

    virtual ~QnSystrayWindow();
    void executeAction(const QString& cmd);

    void showMessage(
        const QString& title,
        const QString& msg,
        QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);

    void handleMessage(const QString& message);

protected:
    virtual void closeEvent(QCloseEvent* event) override;

private slots:
    void onDelayedMessage();

    void iconActivated(QSystemTrayIcon::ActivationReason reason);

    void at_mediaServerStartAction();
    void at_mediaServerStopAction();
    void at_mediaServerWebAction();
    void onShowMediaServerLogAction();

    void findServiceInfo();
    void updateServiceInfo();
    void mediaServerInfoUpdated(quint64 status);

private:
    void updateServiceInfoInternal(quint64 status);

    void createActions();
    void updateActions();
    void createTrayIcon();

    bool isServerInstalled() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
