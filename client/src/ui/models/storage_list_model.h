#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/customization/customized.h>
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

    void setStorages(const QnStorageSpaceDataList& storages);
    void addStorage(const QnStorageSpaceData& data);
    QnStorageSpaceDataList storages() const;

    virtual int rowCount(const QModelIndex &parent) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    bool isBackupRole() const;
    void setBackupRole(bool value);

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
private:
    QnStorageSpaceDataList m_storages;
    bool m_isBackupRole;
    bool m_readOnly;
    QBrush m_linkBrush;
    QFont m_linkFont;
};
