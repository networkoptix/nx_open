#ifndef QN_LOGIN_DIALOG_H
#define QN_LOGIN_DIALOG_H

#include <QtGui/QDialog>

#include <client/client_settings.h>
#include <utils/network/networkoptixmodulefinder.h>
#include <utils/network/foundenterprisecontrollersmodel.h>

#include <ui/workbench/workbench_context_aware.h>

#include "connectinfo.h"

class QDataWidgetMapper;
class QStandardItemModel;
class QUrl;

class QnWorkbenchContext;
class QnAbstractArchiveReader;
class QnVideoCamera;
class QnResourceWidgetRenderer;
class QnRenderingWidget;

namespace Ui {
    class LoginDialog;
}

class QnLoginDialog : public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QDialog base_type;
public:
    explicit QnLoginDialog(QWidget *parent = NULL, QnWorkbenchContext *context = NULL);
    virtual ~QnLoginDialog();

    QUrl currentUrl() const;
    QString currentName() const;
    QnConnectInfoPtr currentInfo() const;

    bool restartPending() const;

    bool rememberPassword() const;

    void setAutoConnect(bool value = true);

public slots:
    virtual void accept() override;
    virtual void reject() override;

protected:
    virtual void changeEvent(QEvent *event) override;
    virtual void showEvent(QShowEvent *event) override;
    virtual void hideEvent(QHideEvent *event) override;

    /**
     * Reset connections model to its initial state. Select last used connection.
     */
    void resetConnectionsModel();

    /**
     * Reset part of connections model containing auto-found controllers.
     */
    void resetAutoFoundConnectionsModel();

    /**
     * Send the required version to the applauncher,
     * close the current instance of the client.
     */
    bool restartInCompatibilityMode(QnConnectInfoPtr connectInfo);

    bool sendCommandToLauncher(const QString &version, const QStringList &arguments);

private slots:
    void updateAcceptibility();
    void updateFocus();
    void updateUsability();

    void at_testButton_clicked();
    void at_saveButton_clicked();
    void at_deleteButton_clicked();
    void at_connectionsComboBox_currentIndexChanged(int index);
    void at_connectFinished(int status, QnConnectInfoPtr connectInfo, int requestHandle);

    void at_entCtrlFinder_remoteModuleFound(const QString& moduleID, const QString& moduleVersion, const TypeSpecificParamMap& moduleParameters, const QString& localInterfaceAddress, const QString& remoteHostAddress, bool isLocal, const QString& seed);
    void at_entCtrlFinder_remoteModuleLost(const QString& moduleID, const TypeSpecificParamMap& moduleParameters, const QString& remoteHostAddress, bool isLocal, const QString& seed );

private:
    QScopedPointer<Ui::LoginDialog> ui;
    QStandardItemModel *m_connectionsModel;
    QDataWidgetMapper *m_dataWidgetMapper;
    int m_requestHandle;
    QnConnectInfoPtr m_connectInfo;

    QnRenderingWidget *m_renderingWidget;
    NetworkOptixModuleFinder *m_entCtrlFinder;

    /** Hash list of automatically found Enterprise Controllers based on seed as key. */
    QMultiHash<QString, QUrl> m_foundEcs;

    bool m_restartPending;
    bool m_autoConnectPending;
};

#endif // LOGINDIALOG_H
