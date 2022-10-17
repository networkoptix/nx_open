// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h>


namespace nx::vms::client::desktop {

/**
 * A list model for global permissions editing in QML.
 */
class GlobalPermissionsModel: public QAbstractListModel
{
    Q_OBJECT

    using base_type = QAbstractListModel;

    /** A subject editing context. */
    Q_PROPERTY(nx::vms::client::desktop::AccessSubjectEditingContext* context
        READ context
        WRITE setContext
        NOTIFY contextChanged)

    Q_PROPERTY(nx::vms::api::GlobalPermissions globalPermissions
        READ ownGlobalPermissions
        WRITE setOwnGlobalPermissions
        NOTIFY globalPermissionsChanged)

public:
    enum Roles
    {
        isChecked = Qt::UserRole + 1,
    };

public:
    explicit GlobalPermissionsModel(QObject* parent = nullptr);
    virtual ~GlobalPermissionsModel() override;

    AccessSubjectEditingContext* context() const;
    void setContext(AccessSubjectEditingContext* value);

    void setOwnGlobalPermissions(nx::vms::api::GlobalPermissions permissions);
    nx::vms::api::GlobalPermissions ownGlobalPermissions() const;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual bool setData(
        const QModelIndex& index,
        const QVariant& value,
        int role = Qt::EditRole) override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    QHash<int, QByteArray> roleNames() const override;

    static void registerQmlTypes();

signals:
    void contextChanged();
    void globalPermissionsChanged();

private:
    void updateInfo();

private:
    AccessSubjectEditingContext* m_context;
    nx::utils::ScopedConnections m_contextConnections;
    nx::vms::api::GlobalPermissions m_ownPermissions;
};

} // namespace nx::vms::client::desktop
