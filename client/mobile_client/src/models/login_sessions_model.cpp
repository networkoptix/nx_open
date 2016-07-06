#include "login_sessions_model.h"

#include <context/login_session.h>

QnLoginSessionsModel::QnLoginSessionsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QnLoginSessionsModel::~QnLoginSessionsModel()
{
}

QVariant QnLoginSessionsModel::data(const QModelIndex &index, int role) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnLoginSession *session = this->session(index.row());

    switch (role) {
    case SessionIdRole:
        return session->id;
    case SystemNameRole:
        return session->systemName;
    case AddressRole:
        return session->address;
    case PortRole:
        return session->port;
    case UserRole:
        return session->user;
    case PasswordRole:
        return session->password;
    }

    return QVariant();
}

QHash<int, QByteArray> QnLoginSessionsModel::roleNames() const {
    QHash<int, QByteArray> roleNames = QAbstractListModel::roleNames();
    roleNames[SessionIdRole] = "sessionId";
    roleNames[SystemNameRole] = "systemName";
    roleNames[AddressRole] = "address";
    roleNames[PortRole] = "port";
    roleNames[UserRole] = "user";
    roleNames[PasswordRole] = "password";
    return roleNames;
}
