// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QSet>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class UserGroupListModel:
    public ScopedModelOperations<QAbstractListModel>,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    enum Columns
    {
        CheckBoxColumn,
        GroupWarningColumn,
        GroupTypeColumn,
        NameColumn,
        DescriptionColumn,
        ParentGroupsColumn,
        PermissionsColumn,

        ColumnCount
    };

    using UserGroupData = nx::vms::api::UserGroupData;
    using UserGroupDataList = nx::vms::api::UserGroupDataList;

    explicit UserGroupListModel(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~UserGroupListModel() override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    const UserGroupDataList& groups() const;
    QSet<nx::Uuid> groupIds() const;
    std::optional<UserGroupData> findGroup(const nx::Uuid& groupId) const;
    int groupRow(const nx::Uuid& groupId) const;

    QSet<nx::Uuid> nonEditableGroupIds() const;

    QSet<nx::Uuid> checkedGroupIds() const;
    void setCheckedGroupIds(const QSet<nx::Uuid>& value);
    void setChecked(const nx::Uuid& groupId, bool checked);

    QSet<nx::Uuid> notFoundGroups() const;
    QSet<nx::Uuid> nonUniqueGroups() const;
    QSet<nx::Uuid> cycledGroups() const;

    void reset(const UserGroupDataList& groups);
    bool addOrUpdateGroup(const UserGroupData& group);
    bool removeGroup(const nx::Uuid& groupId);

    bool canDeleteGroup(const nx::Uuid& groupId) const;

signals:
    void notFoundGroupsChanged();
    void nonUniqueGroupsChanged();
    void cycledGroupsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
