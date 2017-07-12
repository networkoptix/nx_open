#include "ldap_user_list_model.h"

#include <ui/style/skin.h>
#include <ui/style/globals.h>

#include <nx/utils/string.h>
#include <utils/common/ldap.h>

QnLdapUserListModel::QnLdapUserListModel(QObject *parent)
    : base_type(parent)
{}

QnLdapUserListModel::~QnLdapUserListModel() {}

int QnLdapUserListModel::rowCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return m_userList.size();
    return 0;
}

int QnLdapUserListModel::columnCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return ColumnCount;
    return 0;
}

QVariant QnLdapUserListModel::data(const QModelIndex &index, int role) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    QnLdapUser user = m_userList[index.row()];

    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        switch (index.column()) {
        case LoginColumn:
            return user.login;
        case FullNameColumn:
            return user.fullName;
        case EmailColumn:
            return user.email;
        case DnColumn:
            return user.dn;
        default:
            break;
        } // switch (column)
        break;
    case Qt::CheckStateRole:
        if (index.column() == CheckBoxColumn)
            return m_checkedUserLogins.contains(user.login) ? Qt::Checked : Qt::Unchecked;
        break;
    case LoginRole:
        return user.login;
    case LdapUserRole:
        return qVariantFromValue<QnLdapUser>(user);
    default:
        break;
    } // switch (role)

    return QVariant();
}

QVariant QnLdapUserListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Vertical)
        return QVariant();

    if (section >= ColumnCount)
        return QVariant();

    if (role != Qt::DisplayRole)
        return base_type::headerData(section, orientation, role);

    switch (section) {
    case LoginColumn:
        return tr("Login");
    case FullNameColumn:
        return tr("Name");
    case EmailColumn:
        return tr("Email");
    case DnColumn:
        return tr("DN");
    default:
        return QString();
    }
}

Qt::ItemFlags QnLdapUserListModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags flags = Qt::NoItemFlags;

    if (!hasIndex(index.row(), index.column(), index.parent()))
        return flags;

    QnLdapUser user = m_userList[index.row()];
    if (user.login.isEmpty())
        return flags;

    flags |= Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (index.column() == CheckBoxColumn)
        flags |= Qt::ItemIsUserCheckable;

    return flags;
}

int QnLdapUserListModel::userIndex(const QString &login) const {
    auto it = std::find_if(m_userList.begin(), m_userList.end(), [&login](const QnLdapUser &user) {
        return user.login == login;
    });

    if (it == m_userList.end())
        return -1;

    return std::distance(m_userList.begin(), it);
}

Qt::CheckState QnLdapUserListModel::checkState() const {
    if (m_checkedUserLogins.isEmpty())
        return Qt::Unchecked;

    if (m_checkedUserLogins.size() == m_userList.size())
        return Qt::Checked;

    return Qt::PartiallyChecked;
}

void QnLdapUserListModel::setCheckState(Qt::CheckState state, const QString& login)
{
    if (state == Qt::PartiallyChecked)
        return;

    if (login.isEmpty())
    {
        m_checkedUserLogins.clear();
        if (state == Qt::Checked)
        {
            for (const QnLdapUser &user: m_userList)
                m_checkedUserLogins.insert(user.login);
        }
    }
    else
    {
        if (state == Qt::Checked)
            m_checkedUserLogins.insert(login);
        else if (state == Qt::Unchecked)
            m_checkedUserLogins.remove(login);
    }

    if (login.isEmpty())
    {
        emit dataChanged(
            index(0, CheckBoxColumn),
            index(m_userList.size() - 1, CheckBoxColumn),
            {Qt::CheckStateRole});
    }
    else
    {
        auto row = userIndex(login);
        if (row < 0)
            return;

        const auto idx = index(row, CheckBoxColumn);
        emit dataChanged(idx, idx, {Qt::CheckStateRole});
    }
}

void QnLdapUserListModel::setCheckState(Qt::CheckState state, const QSet<QString>& logins)
{
    switch (state)
    {
        case Qt::Checked:
            m_checkedUserLogins.unite(logins);
            break;
        case Qt::Unchecked:
            m_checkedUserLogins.subtract(logins);
            break;
        case Qt::PartiallyChecked:
            return;
        default:
            break;
    }

    if (state == Qt::PartiallyChecked)
        return;

    emit dataChanged(
        index(0, CheckBoxColumn),
        index(m_userList.size() - 1, CheckBoxColumn),
        {Qt::CheckStateRole});
}

QnLdapUsers QnLdapUserListModel::users() const
{
    return m_userList;
}

void QnLdapUserListModel::setUsers(const QnLdapUsers &users) {
    beginResetModel();
    m_userList = users;
    endResetModel();
}
