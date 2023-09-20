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
    QSet<QnUuid> groupIds() const;
    std::optional<UserGroupData> findGroup(const QnUuid& groupId) const;
    int groupRow(const QnUuid& groupId) const;

    QSet<QnUuid> nonEditableGroupIds() const;

    QSet<QnUuid> checkedGroupIds() const;
    void setCheckedGroupIds(const QSet<QnUuid>& value);
    void setChecked(const QnUuid& groupId, bool checked);

    QSet<QnUuid> notFoundGroups() const;
    QSet<QnUuid> nonUniqueGroups() const;
    QSet<QnUuid> cycledGroups() const;

    void reset(const UserGroupDataList& groups);
    bool addOrUpdateGroup(const UserGroupData& group);
    bool removeGroup(const QnUuid& groupId);

    bool canDeleteGroup(const QnUuid& groupId) const;

signals:
    void notFoundGroupsChanged();
    void nonUniqueGroupsChanged();
    void cycledGroupsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
