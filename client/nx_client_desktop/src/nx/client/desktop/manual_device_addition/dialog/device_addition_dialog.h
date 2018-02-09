#pragma once

#include "private/current_system_servers.h"
#include "private/server_online_status_watcher.h"

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <nx/client/desktop/resource_views/models/fake_resource_list_model.h>

class QStackedWidget;

namespace Ui {
class DeviceAdditionDialog;
} // namespace Ui

namespace nx {
namespace client {
namespace desktop {

class ManualDeviceSearcher;

class FoundDevicesModel;

class DeviceAdditionDialog: public Connective<QnSessionAwareDialog>
{
    Q_OBJECT
    using base_type = Connective<QnSessionAwareDialog>;

public:
    DeviceAdditionDialog(QWidget* parent = nullptr);

    virtual ~DeviceAdditionDialog() override;

    void setServer(const QnMediaServerResourcePtr& server);

private:
    using SearcherPtr = QSharedPointer<ManualDeviceSearcher>;

    void initializeControls();
    void setupTable();
    void setupTableHeader();
    void setupPortStuff(QCheckBox* autoCheckbox, QStackedWidget* portStackedWidget);

    void updateProgress();
    void handleStartSearchClicked();
    void stopSearch();
    void handleAddDevicesClicked();
    void cleanUnfinishedSearches(const QnMediaServerResourcePtr& server);
    void removeUnfinishedSearch(const SearcherPtr& searcher);
    void updateResultsWidgetState();
    void handleSearchTypeChanged();
    void handleDialogClosed();
    void updateAddDevicesButtonText();
    void handleServerOnlineStateChanged();
    void updateSelectedServerButtonVisibility();
    void handleSelectedServerChanged(const QnMediaServerResourcePtr& previous);
    void handleModelDataChanged(
        const QModelIndex& topLeft,
        const QModelIndex& bottomRight,
        const QVector<int>& roles);

    void showAdditionFailedDialog(const FakeResourceList& resources);

    int port() const;
    QString password() const;
    QString login() const;
    QString progressMessage() const;

private:
    CurrentSystemServers m_serversWatcher;
    ServerOnlineStatusWatcher m_serverStatusWatcher;

    QScopedPointer<Ui::DeviceAdditionDialog> ui;

    using SearchersList = QList<SearcherPtr>;

    SearcherPtr m_currentSearch;
    SearchersList m_unfinishedSearches;

    QScopedPointer<FoundDevicesModel> m_model;
};

} // namespace desktop
} // namespace client
} // namespace nx
