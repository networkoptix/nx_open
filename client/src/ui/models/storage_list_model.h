#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/customization/customized.h>
#include <ui/workbench/workbench_context_aware.h>
#include "api/model/storage_space_reply.h"

class QnStorageListModelPrivate;
class QnStorageListModel : public Customized<QAbstractListModel>, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef Customized<QAbstractListModel> base_type;

public:
    enum Columns {
        CheckBoxColumn,
        UrlColumn,
        TypeColumn,
        SizeColumn,
        RemoveActionColumn,
        ChangeGroupActionColumn,

        ColumnCount
    };

    QnStorageListModel(QObject *parent = 0);
    ~QnStorageListModel();

    void setModelData(const QnStorageSpaceReply& data);

    virtual int rowCount(const QModelIndex &parent) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    QnStorageSpaceReply m_data;
};
