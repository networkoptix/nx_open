#include "login_sessions_model.h"

#include <algorithm>

#include "mobile_client/mobile_client_settings.h"

QnLoginSessionsModel::QnLoginSessionsModel(QObject *parent) :
    QAbstractListModel(parent)
{
//    loadFromSettings();
    QnLoginSession s;
    s.systemName = lit("DEMO_SYSTEM");
    s.address = lit("127.0.0.1");
    s.port = 7001;
    s.user = lit("admin");
    s.password = lit("123");
    m_savedSessions.append(s);

    s.systemName = lit("regress1");
    s.address = lit("192.168.11.23");
    s.port = 8012;
    s.user = lit("user");
    s.password = lit("123111");
    m_savedSessions.append(s);

    s.systemName = lit("regress123123");
    s.address = lit("192.168.11.211");
    s.port = 8042;
    s.user = lit("admin");
    s.password = lit("123111");
    m_savedSessions.append(s);

}

QnLoginSessionsModel::~QnLoginSessionsModel() {

}

int QnLoginSessionsModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)

    int result = 0;

    if (m_savedSessions.size() > 0)
        result += m_savedSessions.size() + 1;

    if (m_discoveredSessions.size() > 0)
        result += m_discoveredSessions.size() + 1;

    return result;
}

QVariant QnLoginSessionsModel::data(const QModelIndex &index, int role) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnLoginSession &session = this->session(index.row());

    switch (role) {
    case SystemNameRole:
        return session.systemName;
    case AddressRole:
        return session.address;
    case PortRole:
        return session.port;
    case UserRole:
        return session.user;
    case PasswordRole:
        return session.password;
    }

    return QVariant();
}

QHash<int, QByteArray> QnLoginSessionsModel::roleNames() const {
    QHash<int, QByteArray> roleNames = QAbstractListModel::roleNames();
    roleNames[SystemNameRole] = "systemName";
    roleNames[AddressRole] = "address";
    roleNames[PortRole] = "port";
    roleNames[UserRole] = "user";
    roleNames[PasswordRole] = "password";
    return roleNames;
}

void QnLoginSessionsModel::resetSessions(const QList<QnLoginSession> &savedSessions, const QList<QnLoginSession> &discoveredSessions) {
    beginResetModel();
    m_savedSessions = savedSessions;
    m_discoveredSessions = discoveredSessions;
    endResetModel();
}

void QnLoginSessionsModel::updateSession(const QString &address, const int port, const QString &user, const QString &password, const QString &systemName) {
    auto predicate = [&](const QnLoginSession &session) -> bool {
        return session.address == address && session.port == port && session.user == user;
    };

    auto it = std::find_if(m_savedSessions.begin(), m_savedSessions.end(), predicate);
    if (it != m_savedSessions.end()) {
        it->password = password;
        QModelIndex index = this->index(it - m_savedSessions.begin());
        emit dataChanged(index, index);
        return;
    }

//    it = std::find_if(m_discoveredSessions.begin(), m_discoveredSessions.end(), predicate);
//    if (it != m_discoveredSessions.end()) {
//    }

    QnLoginSession session;
    session.systemName = systemName;
    session.address = address;
    session.port = port;
    session.user = user;
    session.password = password;

    beginInsertRows(QModelIndex(), 0, 0);
    m_savedSessions.insert(0, session);
    endInsertRows();

    saveToSettings();
}

int QnLoginSessionsModel::savedSessionIndex(int row) const {
    if (row > 0 && row <= m_savedSessions.size())
        return row - 1;
    return -1;
}

int QnLoginSessionsModel::discoveredSessionIndex(int row) const {
    if (row > m_savedSessions.size() + 1)
        return row - m_savedSessions.size() - 2;
    return -1;
}

QnLoginSession QnLoginSessionsModel::session(int row) const {
    int i = savedSessionIndex(row);
    if (i >= 0)
        return m_savedSessions[i];

    i = discoveredSessionIndex(row);
    if (i >= 0)
        return m_discoveredSessions[i];

    QnLoginSession session;

    if (row == 0)
        session.systemName = tr("Saved sessions");
    else if (row == m_savedSessions.size() + 1)
        session.systemName = tr("Discovered sessions");

    return session;
}

void QnLoginSessionsModel::loadFromSettings() {
    QVariantList variantList = qnSettings->savedSessions();

    QList<QnLoginSession> sessions;
    for (const QVariant &variant: variantList) {
        QnLoginSession session = QnLoginSession::fromVariant(variant.toMap());
        if (session.port == -1)
            continue;

        sessions.append(session);
    }

    resetSessions(sessions, m_discoveredSessions);
}

void QnLoginSessionsModel::saveToSettings() {
    QVariantList variantList;
    for (const QnLoginSession &session: m_savedSessions)
        variantList.append(session.toVariant());
    qnSettings->setSavedSessions(variantList);
}

QVariantMap QnLoginSession::toVariant() const {
    QVariantMap variant;
    variant[lit("systemName")] = systemName;
    variant[lit("address")] = address;
    variant[lit("port")] = port;
    variant[lit("user")] = user;
    variant[lit("password")] = password;
    return variant;
}

QnLoginSession QnLoginSession::fromVariant(const QVariantMap &variant) {
    QnLoginSession session;
    session.systemName = variant[lit("systemName")].toString();
    session.address = variant[lit("address")].toString();
    session.port = variant.value(lit("port"), -1).toInt();
    session.user = variant[lit("user")].toString();
    session.password = variant[lit("password")].toString();
    return session;
}
