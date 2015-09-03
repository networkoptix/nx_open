#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/customization/customized.h>
#include <ui/workbench/workbench_context_aware.h>

class QnUserListModelPrivate;
class QnUserListModel : public Customized<QAbstractListModel>, public QnWorkbenchContextAware {
    Q_OBJECT
    Q_PROPERTY(QnUserManagementColors colors READ colors WRITE setColors)
    typedef Customized<QAbstractListModel> base_type;

public:
    enum Columns {
        CheckBoxColumn,
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

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state, const QnUserResourcePtr &user = QnUserResourcePtr());

    const QnUserManagementColors colors() const;
    void setColors(const QnUserManagementColors &colors);
private:
    QnUserListModelPrivate *d;
    friend class QnUserListModelPrivate;
};


class QnSortedUserListModel : public QSortFilterProxyModel, public QnWorkbenchContextAware {
    typedef QSortFilterProxyModel base_type;

public:
    QnSortedUserListModel(QObject *parent);

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
};
