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
class NewDeviceAdditionDialog;
} // namespace Ui

namespace nx::vms::client::desktop {

class NewSortModel: public QSortFilterProxyModel
{
    Q_OBJECT

public:
    NewSortModel(QObject* parent = nullptr);

private:
    bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override;

private:
    QCollator m_predicate;
};

class ManualDeviceSearcher;
class FoundDevicesModel;

class NewDeviceAdditionDialog: public QnSessionAwareDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareDialog;

public:
    NewDeviceAdditionDialog(QWidget* parent = nullptr);
    virtual ~NewDeviceAdditionDialog() override;

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

    void handleAddressFieldTextChanged(const QString& value);
    void handlePortValueChanged(int value);

    void showAdditionFailedDialog(const FakeResourceList& resources);

    void handleTabClicked(int index);

    void updateAddDevicesPanel();
    void showAddDevicesButton(const QString& buttonText);
    void showAddDevicesPlaceholder(const QString& placeholderText);

    void setDeviceAdded(const QString& physicalId);
    void handleDeviceRemoved(const QString& physicalId);

    std::optional<int> port() const;
    QString password() const;
    QString login() const;
    QString progressMessage() const;

private:
    CurrentSystemServers m_serversWatcher;
    ServerOnlineStatusWatcher m_serverStatusWatcher;

    QScopedPointer<Ui::NewDeviceAdditionDialog> ui;

    using SearchersList = QList<SearcherPtr>;

    SearcherPtr m_currentSearch;
    SearchersList m_unfinishedSearches;

    QScopedPointer<FoundDevicesModel> m_model;
    NewSortModel* m_sortModel = nullptr;
    bool m_addressEditing = false;

    std::optional<int> portIndex;
    std::optional<int> portLength;
};

} // namespace nx::vms::client::desktop
