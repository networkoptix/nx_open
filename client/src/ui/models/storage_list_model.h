#pragma once

#include <array>

#include <QtCore/QSortFilterProxyModel>

#include <api/model/storage_space_reply.h>
#include <api/model/rebuild_archive_reply.h>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <server/server_storage_manager_fwd.h>

#include <ui/customization/customized.h>
#include <ui/models/storage_model_info.h>
#include <ui/workbench/workbench_context_aware.h>


class QnStorageListModel : public Customized<QAbstractListModel> {
    Q_OBJECT
    typedef Customized<QAbstractListModel> base_type;

public:
    enum Columns {
        CheckBoxColumn,
        UrlColumn,
        TypeColumn,
        TotalSpaceColumn,
        RemoveActionColumn,
        ChangeGroupActionColumn,

        ColumnCount
    };

    QnStorageListModel(QObject *parent = 0);
    ~QnStorageListModel();

    void setStorages(const QnStorageModelInfoList& storages);
    void addStorage(const QnStorageModelInfo& storage);
    void updateStorage(const QnStorageModelInfo& storage);
    void removeStorage(const QnStorageModelInfo& storage);

    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr& server);

    QnStorageModelInfo storage(const QModelIndex &index) const;
    QnStorageModelInfoList storages() const;

    void updateRebuildInfo(QnServerStoragesPool pool, const QnStorageScanData &rebuildStatus);

    /** Check if the storage can be moved from this model to another. */
    bool canMoveStorage(const QnStorageModelInfo& data) const;

    /** Check if storage can be removed from the system. */
    bool canRemoveStorage(const QnStorageModelInfo &data) const;

    /**
     *  Check if storage is active on the server.
     *  Inactive storages are:
     *      - newly added remote storages, until changes are applied
     *      - auto-found server-side partitions without storage
     */
    bool storageIsActive(const QnStorageModelInfo &data) const;

    virtual int rowCount(const QModelIndex &parent) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

protected:
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

private:
    QString displayData(const QModelIndex &index, bool forcedText) const;
    QVariant fontData(const QModelIndex &index) const;
    QVariant foregroundData(const QModelIndex &index) const;
    QVariant mouseCursorData(const QModelIndex &index) const;
    QVariant checkstateData(const QModelIndex &index) const;

    void sortStorages();

    /* Check if the whole section is in rebuild. */
    bool isStoragePoolInRebuild(const QnStorageModelInfo& storage) const;

    /* Check if the current storage is in rebuild. */
    bool isStorageInRebuild(const QnStorageModelInfo& storage) const;

private:
    QnMediaServerResourcePtr m_server;
    QnStorageModelInfoList m_storages;
    std::array<QnStorageScanData, static_cast<int>(QnServerStoragesPool::Count)> m_rebuildStatus;

    bool m_readOnly;
    QBrush m_linkBrush;
    QFont m_linkFont;
};
