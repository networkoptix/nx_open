// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <variant>

#include <QtCore/QAbstractListModel>

#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h>


namespace nx::vms::client::desktop {

/**
 * A model for displaying summary information about custom access granted to user or group.
 */
class CustomAccessSummaryModel: public QAbstractListModel
{
    Q_OBJECT

    using base_type = QAbstractListModel;

    Q_PROPERTY(nx::core::access::ResourceAccessMap sharedResources WRITE setOwnSharedResources)
    Q_PROPERTY(nx::vms::api::GlobalPermissions globalPermissions WRITE setOwnGlobalPermissions)

public:
    enum Roles
    {
        isChecked = Qt::UserRole + 1,
    };

public:
    explicit CustomAccessSummaryModel(QObject* parent = nullptr);
    virtual ~CustomAccessSummaryModel() override;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    QHash<int, QByteArray> roleNames() const override;

    void setOwnGlobalPermissions(nx::vms::api::GlobalPermissions permissions);

    void setOwnSharedResources(const nx::core::access::ResourceAccessMap& resources);

    static void registerQmlTypes();

private:
    bool testFlag(const std::variant<api::AccessRight, api::GlobalPermission>& flag) const;
    void updateInfo();

private:
    api::AccessRights m_customRights;
    api::GlobalPermissions m_ownGlobalPermissions;
    bool m_hasDisplayableItems = false;
};

} // namespace nx::vms::client::desktop
