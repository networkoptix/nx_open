#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QAbstractItemModel>

#include <utils/common/connective.h>

#include <nx_ec/data/api_fwd.h>

#include <nx/utils/scoped_model_operations.h>


class QnUserRolesModelPrivate;
class QnUserRolesModel : public ScopedModelOperations<Connective<QAbstractItemModel>>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<Connective<QAbstractItemModel>>;

public:
    explicit QnUserRolesModel(QObject* parent = nullptr, bool standardRoles = true, bool userRoles = true, bool customRole = true);
    virtual ~QnUserRolesModel();

    /* Role-specific stuff: */

    void setStandardRoles(bool enabled = true);
 //   void setStandardRoles(const QList<Qn::GlobalPermissions>& standardRoles);

    void setUserRoles(bool enabled = true);
    void setUserRoles(const ec2::ApiUserGroupDataList& roles);

    bool updateUserRole(const ec2::ApiUserGroupData& role);
    bool removeUserRole(const QnUuid& id);

    void setCustomRole(bool enabled);

    /* QAbstractItemModel implementation: */

    virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex& child) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    QScopedPointer<QnUserRolesModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnUserRolesModel);
};
