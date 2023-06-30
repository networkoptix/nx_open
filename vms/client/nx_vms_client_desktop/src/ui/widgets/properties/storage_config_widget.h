// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QDateTime>
#include <QtCore/QFlags>
#include <QtCore/QModelIndex>
#include <QtWidgets/QWidget>

#include <api/model/api_model_fwd.h>
#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/api/types/resource_types.h>
#include <storage/server_storage_manager_fwd.h>
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
    enum StorageConfigWarningFlag
    {
        analyticsIsOnSystemDrive = 1,
        analyticsIsOnDisabledStorage = 1 << 1,
        hasDisabledStorage = 1 << 2,
        hasUsbStorage = 1 << 3,
        cloudBackupStorageBeingEnabled = 1 << 4,
        backupStorageBeingReplacedByCloudStorage = 1 << 5,
    };
    Q_DECLARE_FLAGS(StorageConfigWarningFlags, StorageConfigWarningFlag)

    StorageConfigWarningFlags storageConfigurationWarningFlags() const;
    void updateWarnings();

private:
    void loadStoragesFromResources(); //< Load initial storages data from resource pool.

    void updateRebuildInfo();
    void atStorageViewClicked(const QModelIndex& index);

    void updateRebuildUi(QnServerStoragesPool pool, const nx::vms::api::StorageScanInfo& reply);

    void confirmNewMetadataStorage(const QnUuid& storageId);

    void startRebuid(bool isMain);
    void cancelRebuild(bool isMain);

    void atServerRebuildStatusChanged(
        const QnMediaServerResourcePtr& server,
        QnServerStoragesPool pool,
        const nx::vms::api::StorageScanInfo& status);

    void atServerRebuildArchiveFinished(
        const QnMediaServerResourcePtr& server,
        QnServerStoragesPool pool);

    void atAddExtStorage(bool addToMain);

    /**
     * @return True if initially disabled cloud backup storage has been enabled. In such case,
     * there is a possibility that cameras backup settings should be updated to be compatible
     * with cloud storage.
     */
    bool cloudStorageToggledOn() const;

    /**
     * Cloud backup storage doesn't support 'All archive' option, so it should be replaced to the
     * 'Motion, Objects, Bookmarks' wherever it needed for cameras on the current server.
     */
    void updateBackupSettingsForCloudStorage();

private:
    void applyStoragesChanges(
        QnStorageResourceList& result,
        const QnStorageModelInfoList& storages) const;

    bool hasStoragesChanges(const QnStorageModelInfoList& storages) const;
    bool isServerOnline() const;

private:
    QScopedPointer<Ui::StorageConfigWidget> ui;
    QnMediaServerResourcePtr m_server;
    QScopedPointer<QnStorageListModel> m_model;
    QScopedPointer<QObject> m_columnResizer;
    QTimer* m_updateStatusTimer;
    QMenu* m_storagePoolMenu;
    QMenu* m_storageArchiveModeMenu;

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
};
