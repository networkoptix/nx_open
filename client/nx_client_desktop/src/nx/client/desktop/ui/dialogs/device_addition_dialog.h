#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <nx/client/desktop/watchers/system_servers_watcher.h>
#include <nx/client/desktop/watchers/server_online_status_watcher.h>

namespace Ui {
class DeviceAdditionDialog;
} // namespace Ui

namespace nx {
namespace client {
namespace desktop {

class ManualDeviceSearcher;

namespace ui {

class FoundDevicesModel;

class DeviceAdditionDialog: public Connective<QnSessionAwareDialog>
{
    Q_OBJECT
    using base_type = Connective<QnSessionAwareDialog>;

public:
    DeviceAdditionDialog(QWidget* parent = nullptr);

    virtual ~DeviceAdditionDialog();

    void setServer(const QnMediaServerResourcePtr& server);

private:
    using SearcherPtr = QSharedPointer<ManualDeviceSearcher>;

    void initializeControls();
    void setupTable();
    void setupTableHeader();
    void setupPortStuff(QCheckBox* autoCheckbox, QSpinBox* portSpinBox);

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

    int port() const;
    QString password() const;
    QString login() const;
    QString progressMessage() const;

private:
    SystemServersWatcher m_serversWatcher;
    ServerOnlineStatusWatcher m_serverStatusWatcher;

    QScopedPointer<Ui::DeviceAdditionDialog> ui;

    using SearchersList = QList<SearcherPtr>;

    SearcherPtr m_currentSearch;
    SearchersList m_unfinishedSearches;

    QScopedPointer<FoundDevicesModel> m_model;


};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
