#pragma once

#include <memory>

#include <QtWidgets/QDialog>

#include <client/client_settings.h>
#include <nx_ec/ec_api.h>
#include <ui/dialogs/common/button_box_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

#include "nx/vms/client/desktop/system_update/compatibility_version_installation_dialog.h"
#include <nx/vms/discovery/manager.h>
#include <nx/vms/api/data/module_information.h>

class QStandardItemModel;
class QStandardItem;
class QUrl;

class QnWorkbenchContext;
class QnAbstractArchiveStreamReader;
class QnResourceWidgetRenderer;
class QnRenderingWidget;

namespace Ui { class LoginDialog; }

class QnLoginDialog:
    public QnButtonBoxDialog,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit QnLoginDialog(QWidget *parent);
    virtual ~QnLoginDialog() override;

public slots:
    virtual void accept() override;
    virtual void reject() override;

protected:
    virtual void changeEvent(QEvent* event) override;
    virtual void showEvent(QShowEvent* event) override;
    virtual void hideEvent(QHideEvent* event) override;

private:
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

private:
    void updateAcceptibility();
    void updateFocus();
    void updateUsability();

    void at_testButton_clicked();
    void at_saveButton_clicked();
    void at_deleteButton_clicked();
    void at_connectionsComboBox_currentIndexChanged(const QModelIndex& index);

    void at_moduleChanged(nx::vms::discovery::ModuleEndpoint data);
    void at_moduleLost(QnUuid id);

    nx::utils::Url currentUrl() const;
    bool isValid() const;

    QStandardItem* newConnectionItem(const QnConnectionData& connection);

    QModelIndex getModelIndexForName(const QString& name);

    static QString defaultLastUsedConnectionName();

    /**
     * @brief Returns name of last used connection for versions less than 3.0
     */
    static QString deprecatedLastUsedConnectionName();

private:
    QScopedPointer<Ui::LoginDialog> ui;
    QStandardItemModel* const m_connectionsModel;
    QStandardItem* m_lastUsedItem = nullptr;
    QStandardItem* m_savedSessionsItem = nullptr;
    QStandardItem* m_autoFoundItem = nullptr;

    int m_requestHandle = -1;

    QnRenderingWidget* const m_renderingWidget;

    struct QnFoundSystemData
    {
        nx::vms::api::ModuleInformation info;
        nx::utils::Url url;

        bool operator==(const QnFoundSystemData& other) const;
        bool operator!=(const QnFoundSystemData& other) const;
    };

    /** Hash list of automatically found Servers based on seed as key. */
    QHash<QnUuid, QnFoundSystemData> m_foundSystems;
};
