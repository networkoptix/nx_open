// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QAbstractItemModel>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>

#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/user_role_data.h>

class QnUserRolesModelPrivate;
class QnUserRolesModel: public ScopedModelOperations<QAbstractItemModel>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractItemModel>;

public:
    enum DisplayRoleFlag
    {
        StandardRoleFlag = 0x00,    /*< Model will use standard roles. */
        UserRoleFlag     = 0x01,    /*< Model will use custom user roles (and listen to changes).*/
        CustomRoleFlag   = 0x02,    /*< Model will display 'Custom...' standard role. */
        AssignableFlag   = 0x04,    /*< Model will display only standard roles current user can assign to others. */
        DefaultRoleFlags = StandardRoleFlag | AssignableFlag | UserRoleFlag | CustomRoleFlag
    };
    Q_DECLARE_FLAGS(DisplayRoleFlags, DisplayRoleFlag)

    enum Column
    {
        NameColumn = 0,
        CheckColumn = 1 //< exists if hasCheckBoxes()
    };

    explicit QnUserRolesModel(QObject* parent = nullptr, DisplayRoleFlags flags = DefaultRoleFlags);
    virtual ~QnUserRolesModel();

    /* Role-specific stuff: */

    int rowForUser(const QnUserResourcePtr& user) const;
    int rowForRole(Qn::UserRole role) const;

    void setUserRoles(const nx::vms::api::UserRoleDataList& roles);

    /* If we want to override "Custom" role name and tooltip. */
    void setCustomRoleStrings(const QString& name, const QString& description);

    /* Controls if UuidRole data contains predefined role pseudo uuids. */
    bool predefinedRoleIdsEnabled() const;
    void setPredefinedRoleIdsEnabled(bool value);

    /* Role selection support. */

    bool hasCheckBoxes() const;
    void setHasCheckBoxes(bool value);

    QSet<QnUuid> checkedRoles() const; //< Returns predefined and user role ids.
    void setCheckedRoles(const QSet<QnUuid>& ids);

    /* QAbstractItemModel implementation. */

    virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex& child) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;

private:
    QScopedPointer<QnUserRolesModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnUserRolesModel)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnUserRolesModel::DisplayRoleFlags);
