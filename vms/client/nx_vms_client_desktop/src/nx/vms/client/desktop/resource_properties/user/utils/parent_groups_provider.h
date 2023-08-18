// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QSortFilterProxyModel>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h>
#include <nx/vms/client/desktop/system_administration/models/members_model.h>

namespace nx::vms::client::desktop {

class ParentGroupsModel: public QSortFilterProxyModel
{
    Q_OBJECT

    using base_type = QSortFilterProxyModel;

    Q_PROPERTY(bool directOnly READ directOnly WRITE setDirectOnly NOTIFY directOnlyChanged)

public:
    ParentGroupsModel(QObject* parent = nullptr);

    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

    bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const override;

    bool directOnly() const
    {
        return m_directOnly;
    }

    void setDirectOnly(bool value);

signals:
    void directOnlyChanged();

private:
    bool m_directOnly = false;
};

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

    Q_PROPERTY(nx::vms::client::desktop::MembersModel* membersModel
        READ membersModel
        WRITE setMembersModel
        NOTIFY membersModelChanged)

public:
    explicit ParentGroupsProvider(QObject* parent = nullptr);
    virtual ~ParentGroupsProvider() override;

    AccessSubjectEditingContext* context() const;
    void setContext(AccessSubjectEditingContext* value);

    nx::vms::client::desktop::MembersModel* membersModel() const { return m_membersModel; }
    void setMembersModel(nx::vms::client::desktop::MembersModel* model);

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
    void membersModelChanged();

private:
    void updateInfo();

private:
    AccessSubjectEditingContext* m_context = nullptr;
    nx::utils::ScopedConnections m_contextConnections;
    nx::vms::client::desktop::MembersModel* m_membersModel = nullptr;
    QList<QnUuid> m_groups;
};

} // namespace nx::vms::client::desktop
