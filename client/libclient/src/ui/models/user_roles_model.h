#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QAbstractItemModel>

#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

#include <nx/utils/scoped_model_operations.h>


class QnUserRolesModelPrivate;
class QnUserRolesModel : public ScopedModelOperations<Connective<QAbstractItemModel>>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<Connective<QAbstractItemModel>>;

public:
    enum DisplayRoleFlag
    {
        StandardRoleFlag = 0x00,    /*< Model will use standard roles. */
        UserRoleFlag     = 0x01,    /*< Model will use custom user roles (and listen to changes).*/
        CustomRoleFlag   = 0x02,    /*< Model will display 'Custom...' standard role. */
        AllRoleFlags     = StandardRoleFlag | UserRoleFlag | CustomRoleFlag
    };
    Q_DECLARE_FLAGS(DisplayRoleFlags, DisplayRoleFlag)

    explicit QnUserRolesModel(QObject* parent = nullptr, DisplayRoleFlags flags = AllRoleFlags);
    virtual ~QnUserRolesModel();

    /* Role-specific stuff: */

    int rowForUser(const QnUserResourcePtr& user) const;
    int rowForRole(Qn::UserRole role) const;

    void setUserRoles(const ec2::ApiUserRoleDataList& roles);

    /* If we want to override "Custom" role name and tooltip: */
    void setCustomRoleStrings(const QString& name, const QString& description);

    /* QAbstractItemModel implementation: */

    virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex& child) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    QScopedPointer<QnUserRolesModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnUserRolesModel)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnUserRolesModel::DisplayRoleFlags);
