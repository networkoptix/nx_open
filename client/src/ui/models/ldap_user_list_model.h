#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <utils/common/ldap_fwd.h>

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

    QnLdapUsers users() const;
    void setUsers(const QnLdapUsers &users);    
private:
    int userIndex(const QString &login) const;

    QnLdapUsers m_userList;
    QSet<QString> m_checkedUserLogins;
};
