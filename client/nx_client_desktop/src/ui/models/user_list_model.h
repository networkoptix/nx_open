#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/customization/customized.h>
#include <ui/workbench/workbench_context_aware.h>


class QnUserListModelPrivate;
class QnUserListModel : public QAbstractListModel, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QAbstractListModel base_type;

public:
    enum Columns {
        CheckBoxColumn,
        UserTypeColumn,
        LoginColumn,
        FullNameColumn,
        UserRoleColumn,
        EnabledColumn,

        ColumnCount
    };

    QnUserListModel(QObject* parent = nullptr);
    virtual ~QnUserListModel();

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual int columnCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state, const QnUserResourcePtr& user = QnUserResourcePtr());

    bool isUserEnabled(const QnUserResourcePtr& user) const;
    void setUserEnabled(const QnUserResourcePtr& user, bool enabled);

    QnUserResourceList users() const;
    void resetUsers(const QnUserResourceList& value);
    void addUser(const QnUserResourcePtr& user);
    void removeUser(const QnUserResourcePtr& user);

    static bool isInteractiveColumn(int column);

private:
    QnUserListModelPrivate* d;
    friend class QnUserListModelPrivate;
};

class QnSortedUserListModel : public QSortFilterProxyModel
{
    typedef QSortFilterProxyModel base_type;

public:
    QnSortedUserListModel(QObject* parent);

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const;
};
