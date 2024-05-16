// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <optional>

#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QBrush>

#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/api/data/storage_scan_info.h>
#include <nx/vms/api/data/storage_space_reply.h>
#include <storage/server_storage_manager_fwd.h>

#include <ui/models/storage_model_info.h>
#include <ui/workbench/workbench_context_aware.h>

class QnStorageListModel: public ScopedModelOperations<QAbstractListModel>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    enum Columns
    {
        UrlColumn,
        TypeColumn,
        StoragePoolColumn,
        StorageArchiveModeWarningIconColumn,
        StorageArchiveModeColumn,
        TotalSpaceColumn,
        ActionsColumn,
        SpacerColumn,
        CheckBoxColumn,

        ColumnCount
    };

    enum Roles
    {
        ShowTextOnHoverRole = Qt::UserRole + 1,
    };

    QnStorageListModel(QObject* parent = nullptr);
    virtual ~QnStorageListModel();

    /**
     * @return Translated lexical representation of the given storage archive mode enumeration
     *     value in a title case.
     */
    static QString toString(nx::vms::api::StorageArchiveMode archiveMode);

    void setStorages(const QnStorageModelInfoList& storages);
    void addStorage(const QnStorageModelInfo& storage);
    bool updateStorage(const QnStorageModelInfo& storage);
    void removeStorage(const QnStorageModelInfo& storage);

    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr& server);

    nx::Uuid metadataStorageId() const;
    void setMetadataStorageId(const nx::Uuid &id);

    QnStorageModelInfo storage(const QModelIndex& index) const;
    QnStorageModelInfoList storages() const;

    std::optional<QnStorageModelInfo> cloudBackupStorage() const;

    void updateRebuildInfo(
        QnServerStoragesPool pool,
        const nx::vms::api::StorageScanInfo& rebuildStatus);

    bool canChangeStoragePool(const QnStorageModelInfo& data) const;
    bool canChangeStorageArchiveMode(const QnStorageModelInfo& data) const;

    /** Check if storage can be removed from the system. */
    bool canRemoveStorage(const QnStorageModelInfo& data) const;

    /**
     * Check if storage could be used to store analytics metadata (i.e. has required storage type).
     * Actual access rights are not checked by this call, since in some scenarios they could be set
     * later by the caller.
     */
    bool couldStoreAnalytics(const QnStorageModelInfo& data) const;

    /**
     * Check if storage is active on the server.
     * Inactive storages are:
     *     - newly added remote storages, until changes are applied
     *     - auto-found server-side partitions without storage
     */
    bool storageIsActive(const QnStorageModelInfo& data) const;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    using ServersByExternalStorageArchiveMode =
        std::map<nx::vms::api::StorageArchiveMode, QnMediaServerResourceList>;
    /**
     * @param storageInfo Record that represents one of external storages presented in this model.
     * @return List of servers, that use external storages with the same URLs as in the storage
     *     description passed as a parameter. The list of servers is broken down according to
     *     storage archive modes that are used to access corresponding storages. The server
     *     presented by the model is also present in the result.
     */
    ServersByExternalStorageArchiveMode serversUsingSameExternalStorage(
        const QnStorageModelInfo& storageInfo) const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

protected:
    virtual bool setData(
        const QModelIndex& index,
        const QVariant& value,
        int role = Qt::EditRole) override;

private:
    QVariant storageArchiveModeWarningIconColumnData(const QModelIndex& index, int role) const;
    QString displayData(const QModelIndex& index, bool forcedText) const;
    QVariant mouseCursorData(const QModelIndex& index) const;
    QVariant checkstateData(const QModelIndex& index) const;
    bool showTextOnHover(const QModelIndex& index) const;

    /* Check if the whole section is in rebuild. */
    bool isStoragePoolInRebuild(const QnStorageModelInfo& storage) const;

    /* Check if the current storage is in rebuild. */
    bool isStorageInRebuild(const QnStorageModelInfo& storage) const;

private:
    QnMediaServerResourcePtr m_server;
    QnStorageModelInfoList m_storages;
    QSet<nx::Uuid> m_checkedStorages;
    std::array<nx::vms::api::StorageScanInfo, static_cast<int>(QnServerStoragesPool::Count)> m_rebuildStatus;
    nx::Uuid m_metadataStorageId;

    bool m_readOnly;
    QBrush m_linkBrush;
};
