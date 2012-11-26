#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QtGui/QDialog>
#include "connectinfo.h"
#include <utils/settings.h>
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include <utils/network/networkoptixmodulefinder.h>
#include <utils/network/foundenterprisecontrollersmodel.h>

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

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QnWorkbenchContext *context, QWidget *parent = 0);
    virtual ~LoginDialog();

    QUrl currentUrl() const;
    QString currentName() const;
    QnConnectInfoPtr currentInfo() const;

public slots:
    virtual void accept() override;
    virtual void reject() override;

protected:
    virtual void changeEvent(QEvent *event) override;

    /**
     * Reset connections model to its initial state. Select last used connection.
     */
    void resetConnectionsModel();

private slots:
    void updateAcceptibility();
    void updateFocus();
    void updateUsability();

    void at_testButton_clicked();
    void at_saveButton_clicked();
    void at_deleteButton_clicked();
    void at_connectionsComboBox_currentIndexChanged(int index);
    void at_connectFinished(int status, const QByteArray &errorString, QnConnectInfoPtr connectInfo, int requestHandle);

    void at_entCtrlFinder_remoteModuleFound(const QString& moduleID, const QString& moduleVersion, const TypeSpecificParamMap& moduleParameters, const QString& localInterfaceAddress,
        const QString& remoteHostAddress, bool isLocal, const QString& seed);
    void at_entCtrlFinder_remoteModuleLost(const QString& moduleID, const TypeSpecificParamMap& moduleParameters, const QString& remoteHostAddress, bool isLocal, const QString& seed );

private:
    Q_DISABLE_COPY(LoginDialog)

    QScopedPointer<Ui::LoginDialog> ui;
    QWeakPointer<QnWorkbenchContext> m_context;
    QStandardItemModel *m_connectionsModel;
    QDataWidgetMapper *m_dataWidgetMapper;
    int m_requestHandle;
    QnConnectInfoPtr m_connectInfo;

    QnRenderingWidget* m_renderingWidget;
    NetworkOptixModuleFinder* m_entCtrlFinder;

    /**
     * @brief m_foundEcs    Hash list of automatically found Enterprise Controllers based on seed as key.
     */
    QMultiHash<QString, QUrl> m_foundEcs;
};

#endif // LOGINDIALOG_H
