#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/customization/customized.h>
#include <ui/models/storage_model_info.h>
#include <ui/workbench/workbench_context_aware.h>
#include "api/model/storage_space_reply.h"

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

    QnStorageModelInfo storage(const QModelIndex &index) const;
    QnStorageModelInfoList storages() const;

    /** Check if the storage can be moved from this model to another. */
    bool canMoveStorage(const QnStorageModelInfo& data) const;

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
private:
    QnStorageModelInfoList m_storages;
    bool m_readOnly;
    QBrush m_linkBrush;
    QFont m_linkFont;
};
