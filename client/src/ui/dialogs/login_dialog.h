#ifndef QN_LOGIN_DIALOG_H
#define QN_LOGIN_DIALOG_H

#include <memory>

#include <QtWidgets/QDialog>

#include <api/model/connection_info.h>
#include <nx_ec/ec_api.h>

#include <client/client_settings.h>
#include <ui/workbench/workbench_context_aware.h>

#include "compatibility_version_installation_dialog.h"


class QStandardItemModel;
class QStandardItem;
class QUrl;

class QnWorkbenchContext;
class QnAbstractArchiveReader;
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
     * Reset part of connections model containing saved sessions.
     */
    void resetSavedSessionsModel();

    /**
     * Reset part of connections model containing auto-found controllers.
     */
    void resetAutoFoundConnectionsModel();

private slots:
    void updateAcceptibility();
    void updateFocus();
    void updateUsability();

    void at_testButton_clicked();
    void at_saveButton_clicked();
    void at_deleteButton_clicked();
    void at_connectionsComboBox_currentIndexChanged(const QModelIndex &index);

    void at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation);

private:
    QUrl currentUrl() const;
    QString currentName() const;

    /** Save current connection credentials. */
    void updateStoredConnections(const QUrl &url, const QString &name);
private:
    QScopedPointer<Ui::LoginDialog> ui;
    QStandardItemModel *m_connectionsModel;
    QStandardItem* m_lastUsedItem;
    QStandardItem* m_savedSessionsItem;
    QStandardItem* m_autoFoundItem;

    int m_requestHandle;

    QnRenderingWidget *m_renderingWidget;

    struct QnEcData {
        QUuid id;
        QUrl url;
        QString version;
        QString systemName;

        bool operator==(const QnEcData& other) const  {
            return id == other.id && url == other.url && version == other.version && systemName == other.systemName;
        }
    };

    /** Hash list of automatically found Servers based on seed as key. */
    QMultiHash<QUuid, QnEcData> m_foundEcs;
};

#endif // LOGINDIALOG_H
