#pragma once

#include "private/current_system_servers.h"
#include "private/server_online_status_watcher.h"

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <nx/vms/client/desktop/resource_views/models/fake_resource_list_model.h>
#include <utils/common/credentials.h>

class QStackedWidget;
class QnResourcePool;

namespace Ui {
class DeviceAdditionDialog;
} // namespace Ui

namespace nx::vms::client::desktop {

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
    void setMainPageActive();

    virtual bool eventFilter(QObject *object, QEvent *event) override;

private:
    using SearcherPtr = QSharedPointer<ManualDeviceSearcher>;

    void initializeControls();
    void setupTable();
    void setupTableHeader();
    void setupPortStuff(QCheckBox* autoCheckbox, QStackedWidget* portStackedWidget);
    void setupSearchResultsPlaceholder();
    void setSearchAccent(bool isEnabled);

    void updateProgress();
    void handleStartSearchClicked();
    void stopSearch();
    void handleAddDevicesClicked();
    void cleanUnfinishedSearches(const QnMediaServerResourcePtr& server);
    void removeUnfinishedSearch(const SearcherPtr& searcher);
    void updateResultsWidgetState();
    void handleSearchTypeChanged();
    void handleDialogClosed();
    void handleServerOnlineStateChanged();
    void updateSelectedServerButtonVisibility();
    void handleSelectedServerChanged(const QnMediaServerResourcePtr& previous);

    void handleStartAddressFieldTextChanged(const QString& value);
    void handleStartAddressEditingFinished();
    void handleEndAddressFieldTextChanged(const QString& value);

    void showAdditionFailedDialog(const FakeResourceList& resources);

    void handleTabClicked(int index);

    void updateAddDevicesPanel();
    void showAddDevicesButton(const QString& buttonText);
    void showAddDevicesPlaceholder(const QString& placeholderText);

    void setDeviceAdded(const QString& uniqueId);
    void handleDeviceRemoved(const QString& uniqueId);

    std::optional<int> port() const;
    QString password() const;
    QString login() const;
    QString progressMessage() const;

private:
    const QPointer<QnResourcePool> m_pool;
    CurrentSystemServers m_serversWatcher;
    ServerOnlineStatusWatcher m_serverStatusWatcher;

    QScopedPointer<Ui::DeviceAdditionDialog> ui;

    using SearchersList = QList<SearcherPtr>;

    SearcherPtr m_currentSearch;
    SearchersList m_unfinishedSearches;
    QString m_lastSearchLogin;
    QString m_lastSearchPassword;

    QScopedPointer<FoundDevicesModel> m_model;
    bool m_addressEditing = false;

    nx::vms::common::Credentials m_knownAddressCredentials;
    nx::vms::common::Credentials m_subnetScanCredentials;
};

} // namespace nx::vms::client::desktop
