#pragma once

#include <array>

#include <QtCore/QSortFilterProxyModel>

#include <api/model/storage_space_reply.h>
#include <api/model/rebuild_archive_reply.h>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <nx/utils/scoped_model_operations.h>

#include <server/server_storage_manager_fwd.h>

#include <ui/customization/customized.h>
#include <ui/models/storage_model_info.h>
#include <ui/workbench/workbench_context_aware.h>


class QnStorageListModel : public ScopedModelOperations<Customized<QAbstractListModel>>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<Customized<QAbstractListModel>>;

public:
    enum Columns
    {
        UrlColumn,
        TypeColumn,
        StoragePoolColumn,
        TotalSpaceColumn,
        RemoveActionColumn,
        CheckBoxColumn,

        ColumnCount
    };

    QnStorageListModel(QObject* parent = nullptr);
    virtual ~QnStorageListModel();

    void setStorages(const QnStorageModelInfoList& storages);
    void addStorage(const QnStorageModelInfo& storage);
    bool updateStorage(const QnStorageModelInfo& storage);
    void removeStorage(const QnStorageModelInfo& storage);

    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr& server);

    QnStorageModelInfo storage(const QModelIndex& index) const;
    QnStorageModelInfoList storages() const;

    void updateRebuildInfo(QnServerStoragesPool pool, const QnStorageScanData& rebuildStatus);

    /** Check if the storage can be moved from this model to another. */
    bool canChangeStoragePool(const QnStorageModelInfo& data) const;

    /** Check if storage can be removed from the system. */
    bool canRemoveStorage(const QnStorageModelInfo& data) const;

    /**
     *  Check if storage is active on the server.
     *  Inactive storages are:
     *      - newly added remote storages, until changes are applied
     *      - auto-found server-side partitions without storage
     */
    bool storageIsActive(const QnStorageModelInfo& data) const;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

protected:
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

private:
    QString displayData(const QModelIndex& index, bool forcedText) const;
    QVariant mouseCursorData(const QModelIndex& index) const;
    QVariant checkstateData(const QModelIndex& index) const;

    /* Check if the whole section is in rebuild. */
    bool isStoragePoolInRebuild(const QnStorageModelInfo& storage) const;

    /* Check if the current storage is in rebuild. */
    bool isStorageInRebuild(const QnStorageModelInfo& storage) const;

private:
    QnMediaServerResourcePtr m_server;
    QnStorageModelInfoList m_storages;
    QSet<QnUuid> m_checkedStorages;
    std::array<QnStorageScanData, static_cast<int>(QnServerStoragesPool::Count)> m_rebuildStatus;

    bool m_readOnly;
    QBrush m_linkBrush;
};
