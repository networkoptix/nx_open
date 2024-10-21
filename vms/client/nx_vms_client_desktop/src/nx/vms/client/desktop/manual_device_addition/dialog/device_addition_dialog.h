// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCollator>
#include <QtCore/QSortFilterProxyModel>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/credentials.h>
#include <nx/vms/client/desktop/resource_views/models/fake_resource_list_model.h>
#include <ui/dialogs/common/session_aware_dialog.h>

#include "private/current_system_servers.h"
#include "private/server_online_status_watcher.h"

class QStackedWidget;
class QnResourcePool;

namespace Ui {
class DeviceAdditionDialog;
} // namespace Ui

namespace nx::vms::client::desktop {

class SortModel: public QSortFilterProxyModel
{
    Q_OBJECT

public:
    SortModel(QObject* parent = nullptr);

private:
    bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override;

private:
    QCollator m_predicate;
};

class ManualDeviceSearcher;
class FoundDevicesModel;

class DeviceAdditionDialog: public QnSessionAwareDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareDialog;

public:
    DeviceAdditionDialog(QWidget* parent = nullptr);
    virtual ~DeviceAdditionDialog() override;

    void setServer(const QnMediaServerResourcePtr& server);

    virtual bool eventFilter(QObject *object, QEvent *event) override;

private:
    using SearcherPtr = QSharedPointer<ManualDeviceSearcher>;

    void initializeControls();
    void setupTable();
    void setupTableHeader();
    void setupPortStuff(QCheckBox* autoCheckbox, QStackedWidget* portStackedWidget);
    void setupSearchResultsPlaceholder();
    void setAddDevicesAccent(bool isEnabled);

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

    void setDeviceAdded(const QString& physicalId);
    void handleDeviceRemoved(const QString& physicalId);
    void setLimitReached();

    std::optional<int> port() const;
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
    SortModel* m_sortModel = nullptr;
    bool m_addressEditing = false;

    nx::vms::api::Credentials m_knownAddressCredentials;
    nx::vms::api::Credentials m_subnetScanCredentials;
};

} // namespace nx::vms::client::desktop
