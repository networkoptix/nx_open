#pragma once

#include <memory>

#include <QtWidgets/QDialog>

#include <nx_ec/ec_api.h>

#include <client/client_settings.h>
#include <ui/dialogs/common/dialog.h>
#include <ui/workbench/workbench_context_aware.h>

#include "compatibility_version_installation_dialog.h"


class QStandardItemModel;
class QStandardItem;
class QUrl;

class QnWorkbenchContext;
class QnAbstractArchiveStreamReader;
class QnResourceWidgetRenderer;
class QnRenderingWidget;

namespace Ui {
class LoginDialog;
}

class QnLoginDialog: public QnDialog, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QnDialog base_type;
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
    void at_moduleFinder_moduleLost(const QnModuleInformation &moduleInformation);

private:
    QUrl currentUrl() const;
    QString currentName() const;

    QString gatherSystemName(const QUrl& url);

    QStandardItem* newConnectionItem(const QnUserRecentConnectionData& connection);

private:
    QScopedPointer<Ui::LoginDialog> ui;
    QStandardItemModel *m_connectionsModel;
    QStandardItem* m_lastUsedItem;
    QStandardItem* m_savedSessionsItem;
    QStandardItem* m_autoFoundItem;

    int m_requestHandle;

    QnRenderingWidget *m_renderingWidget;

    struct QnFoundSystemData
    {
        QnModuleInformation info;
        QUrl url;

        bool operator==(const QnFoundSystemData& other) const;
        bool operator!=(const QnFoundSystemData& other) const;
    };

    /** Hash list of automatically found Servers based on seed as key. */
    QHash<QnUuid, QnFoundSystemData> m_foundSystems;
};
