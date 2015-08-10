#pragma once

#include <QtCore/QAbstractListModel>
#include <ui/workbench/workbench_context_aware.h>

class QnUserListModelPrivate;
class QnUserListModel : public QAbstractListModel, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QAbstractListModel base_type;

public:
    enum Columns {
        NameColumn,
        PermissionsColumn,
        LdapColumn,
        EnabledColumn,
        EditIconColumn,

        ColumnCount
    };

    QnUserListModel(QObject *parent = 0);
    ~QnUserListModel();

    virtual int rowCount(const QModelIndex &parent) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    QnUserListModelPrivate *d;
    friend class QnUserListModelPrivate;
};
