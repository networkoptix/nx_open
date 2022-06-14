// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QSet>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/api/data/user_role_data.h>

namespace nx::vms::client::desktop {

class UserGroupListModel: public ScopedModelOperations<QAbstractListModel>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    enum Columns
    {
        CheckBoxColumn,
        GroupTypeColumn,
        NameColumn,
        DescriptionColumn,
        ParentGroupsColumn,
        PermissionsColumn,

        ColumnCount
    };

    using UserGroupData = nx::vms::api::UserRoleData;
    using UserGroupDataList = nx::vms::api::UserRoleDataList;

    explicit UserGroupListModel(QObject* parent = nullptr);
    virtual ~UserGroupListModel() override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    const UserGroupDataList& groups() const;
    QSet<QnUuid> groupIds() const;

    QSet<QnUuid> checkedGroupIds() const;
    void setCheckedGroupIds(const QSet<QnUuid>& value);

    void reset(const UserGroupDataList& groups);
    bool addOrUpdateGroup(const UserGroupData& group);
    bool removeGroup(const QnUuid& groupId);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
