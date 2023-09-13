// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QDateTime>
#include <QtCore/QModelIndex>
#include <QtWidgets/QWidget>

#include <api/model/api_model_fwd.h>
#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/api/types/resource_types.h>
#include <server/server_storage_manager_fwd.h>
#include <ui/models/storage_model_info.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class StorageConfigWidget; }
class QMenu;
class QnStorageListModel;

class QnStorageConfigWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    QnStorageConfigWidget(QWidget* parent = nullptr);
    virtual ~QnStorageConfigWidget() override;

    virtual void loadDataToUi() override;

    void setServer(const QnMediaServerResourcePtr& server);
    virtual void applyChanges() override;
    virtual void discardChanges() override;
    virtual bool hasChanges() const override;
    virtual bool isNetworkRequestRunning() const override;

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;
    virtual void showEvent(QShowEvent* event) override;
    virtual void hideEvent(QHideEvent* event) override;

private:
    // Load initial storages data from resource pool.
    void loadStoragesFromResources();

    void updateRebuildInfo();
    void at_storageView_clicked(const QModelIndex& index);

    void updateRebuildUi(QnServerStoragesPool pool, const nx::vms::api::StorageScanInfo& reply);

    void confirmNewMetadataStorage(const QnUuid& storageId);

    void updateWarnings();

    void startRebuid(bool isMain);
    void cancelRebuild(bool isMain);

    void at_serverRebuildStatusChanged(const QnMediaServerResourcePtr& server,
        QnServerStoragesPool pool, const nx::vms::api::StorageScanInfo& status);

    void at_serverRebuildArchiveFinished(const QnMediaServerResourcePtr& server,
        QnServerStoragesPool pool);

    void at_addExtStorage(bool addToMain);

private:
    QScopedPointer<Ui::StorageConfigWidget> ui;
    QnMediaServerResourcePtr m_server;
    QScopedPointer<QnStorageListModel> m_model;
    QScopedPointer<QObject> m_columnResizer;
    QTimer* m_updateStatusTimer;
    QMenu* m_storagePoolMenu;

    struct StoragePool
    {
        bool rebuildCancelled = false;
    };

    StoragePool m_mainPool;
    StoragePool m_backupPool;

    bool m_updating = false;

    class MetadataWatcher;
    QScopedPointer<MetadataWatcher> m_metadataWatcher;

    rest::Handle m_currentRequest = 0;
private:
    void applyStoragesChanges(QnStorageResourceList& result,
        const QnStorageModelInfoList& storages) const;
    bool hasStoragesChanges(const QnStorageModelInfoList& storages) const;
    bool isServerOnline() const;
};
