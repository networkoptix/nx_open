// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QSet>

#include <nx/vms/api/data/ldap.h>

class QnLdapUserListModel : public QAbstractListModel {
    Q_OBJECT
    typedef QAbstractListModel base_type;

public:
    enum Columns {
        CheckBoxColumn,
        LoginColumn,
        FullNameColumn,
        EmailColumn,
        DnColumn,

        ColumnCount
    };

    enum DataRole {
        LoginRole = Qt::UserRole + 1,
        LdapUserRole
    };

    QnLdapUserListModel(QObject *parent = 0);
    ~QnLdapUserListModel();

    virtual int rowCount(const QModelIndex &parent) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state, const QString &login = QString());
    void setCheckState(Qt::CheckState state, const QSet<QString>& logins);

    nx::vms::api::LdapUserList users() const;
    void setUsers(const nx::vms::api::LdapUserList& users);

private:
    int userIndex(const QString &login) const;

    nx::vms::api::LdapUserList m_userList;
    QSet<QString> m_checkedUserLogins;
};
