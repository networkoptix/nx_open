// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringListModel>

#include <utils/common/connective.h>

class AbstractSystemsController;
class QnSystemsModelPrivate;

class QnSystemsModel: public Connective<QAbstractListModel>
{
    Q_OBJECT

    using base_type = Connective<QAbstractListModel>;

public:
    enum RoleId
    {
        FirstRoleId = Qt::UserRole + 1,

        SearchRoleId = FirstRoleId,
        SystemNameRoleId,
        SystemIdRoleId,
        LocalIdRoleId,

        OwnerDescriptionRoleId,

        IsFactorySystemRoleId,

        IsCloudSystemRoleId,
        IsOnlineRoleId,
        IsReachableRoleId,
        IsCompatibleToMobileClient,
        IsCompatibleVersionRoleId,
        IsCompatibleToDesktopClient,

        WrongCustomizationRoleId,
        WrongVersionRoleId,
        CompatibleVersionRoleId,

        VisibilityScopeRoleId,

        RolesCount
    };

public:
    QnSystemsModel(AbstractSystemsController* controller, QObject* parent = nullptr);
    virtual ~QnSystemsModel();

public: // overrides
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;

    int getRowIndex(const QString& systemId) const;

private:
    QScopedPointer<QnSystemsModelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnSystemsModel)
};
