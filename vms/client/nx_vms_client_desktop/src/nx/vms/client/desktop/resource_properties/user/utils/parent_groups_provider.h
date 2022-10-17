// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h>


namespace nx::vms::client::desktop {

/**
 * An utility class to obtain information summary about parent groups in a QML.
 */
class ParentGroupsProvider: public QAbstractListModel
{
    Q_OBJECT

    using base_type = QAbstractListModel;

    /** A subject editing context. */
    Q_PROPERTY(nx::vms::client::desktop::AccessSubjectEditingContext* context
        READ context
        WRITE setContext
        NOTIFY contextChanged)

public:
    enum Roles
    {
        isParent = Qt::UserRole + 1,
        isLdap,
        isPredefined,
    };

public:
    explicit ParentGroupsProvider(QObject* parent = nullptr);
    virtual ~ParentGroupsProvider() override;

    AccessSubjectEditingContext* context() const;
    void setContext(AccessSubjectEditingContext* value);

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

private:
    void updateInfo();

private:
    AccessSubjectEditingContext* m_context;
    nx::utils::ScopedConnections m_contextConnections;
    QList<QnUuid> m_groups;
};

} // namespace nx::vms::client::desktop
