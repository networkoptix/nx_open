// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QSortFilterProxyModel>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class UserListModel:
    public QAbstractListModel,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QAbstractListModel;

public:
    enum Columns
    {
        CheckBoxColumn,
        UserTypeColumn,
        LoginColumn,
        FullNameColumn,
        EmailColumn,
        UserGroupsColumn,
        IsCustomColumn,

        ColumnCount
    };

    explicit UserListModel(QObject* parent = nullptr);
    virtual ~UserListModel() override;

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual int columnCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state, const QnUserResourcePtr& user = QnUserResourcePtr());

    bool isUserEnabled(const QnUserResourcePtr& user) const;
    void setUserEnabled(const QnUserResourcePtr& user, bool enabled);
    bool isDigestEnabled(const QnUserResourcePtr& user) const;
    void setDigestEnabled(const QnUserResourcePtr& user, bool enabled);

    QnUserResourceList users() const;
    void resetUsers(const QnUserResourceList& value);
    void addUser(const QnUserResourcePtr& user);
    void removeUser(const QnUserResourcePtr& user);

    static bool isInteractiveColumn(int column);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

class SortedUserListModel: public QSortFilterProxyModel
{
    using base_type = QSortFilterProxyModel;

public:
    explicit SortedUserListModel(QObject* parent = nullptr);

    void setDigestFilter(std::optional<bool> value);

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const;
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    std::optional<bool> m_digestFilter;
};

} // namespace nx::vms::client::desktop
