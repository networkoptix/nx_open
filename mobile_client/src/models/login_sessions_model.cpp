#include "login_sessions_model.h"

#include <algorithm>

#include "mobile_client/mobile_client_settings.h"
#include "utils/network/multicast_module_finder.h"

QnLoginSessionsModel::QnLoginSessionsModel(QObject *parent) :
    QAbstractListModel(parent),
    m_moduleFinder(new QnMulticastModuleFinder(true))
{
    loadFromSettings();

    m_moduleFinder->start();
    connect(m_moduleFinder.data(),  &QnMulticastModuleFinder::moduleAddressFound,   this,   &QnLoginSessionsModel::at_moduleFinder_moduleAddressFound);
    connect(m_moduleFinder.data(),  &QnMulticastModuleFinder::moduleAddressLost,    this,   &QnLoginSessionsModel::at_moduleFinder_moduleAddressLost);

//    QnLoginSession s;
//    s.systemName = lit("DEMO_SYSTEM");
//    s.address = lit("127.0.0.1");
//    s.port = 7001;
//    s.user = lit("admin");
//    s.password = lit("123");
//    m_savedSessions.append(s);

//    s.systemName = lit("regress1");
//    s.address = lit("192.168.11.23");
//    s.port = 8012;
//    s.user = lit("user");
//    s.password = lit("123111");
//    m_savedSessions.append(s);

//    s.systemName = lit("regress123123");
//    s.address = lit("192.168.11.211");
//    s.port = 8042;
//    s.user = lit("admin");
//    s.password = lit("123111");
//    m_savedSessions.append(s);


}

QnLoginSessionsModel::~QnLoginSessionsModel() {
    m_moduleFinder->stop();
}

int QnLoginSessionsModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)

    return m_savedSessions.size() + m_discoveredSessions.size();
}

QVariant QnLoginSessionsModel::data(const QModelIndex &index, int role) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnLoginSession &session = this->session(index.row());

    switch (role) {
    case SessionIdRole:
        return session.id();
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
    case SectionRole:
        return savedSessionIndex(index.row()) != -1 ? tr("Saved sessions") : tr("Discovered servers");
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
    roleNames[SectionRole] = "section";
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
        it->systemName = systemName;

        int row = it - m_savedSessions.begin();
        beginMoveRows(QModelIndex(), row, row, QModelIndex(), 0);
        m_savedSessions.move(row, 0);
        endInsertRows();
    } else {
        QnLoginSession session;
        session.systemName = systemName;
        session.address = address;
        session.port = port;
        session.user = user;
        session.password = password;

        beginInsertRows(QModelIndex(), 0, 0);
        m_savedSessions.insert(0, session);
        endInsertRows();
    }

    saveToSettings();
}

void QnLoginSessionsModel::deleteSession(const QString &id) {
    for (int i = 0; i < m_savedSessions.size(); i++) {
        if (m_savedSessions[i].id() != id)
            continue;

        beginRemoveRows(QModelIndex(), i, i);
        m_savedSessions.removeAt(i);
        endRemoveRows();
    }

    saveToSettings();
}

void QnLoginSessionsModel::at_moduleFinder_moduleAddressFound(const QnModuleInformation &moduleInformation, const QnNetworkAddress &address) {
    bool found = false;
    for (const QnLoginSession &session: m_discoveredSessions) {
        if (session.address == address.host().toString() && session.port == address.port()) {
            found = true;
            break;
        }
    }

    if (found)
        return;

    QnLoginSession session;
    session.address = address.host().toString();
    session.port = address.port();
    session.systemName = moduleInformation.systemName;

    int row = rowCount();
    beginInsertRows(QModelIndex(), row, row);
    m_discoveredSessions.append(session);
    endInsertRows();
}

void QnLoginSessionsModel::at_moduleFinder_moduleAddressLost(const QnModuleInformation &moduleInformation, const QnNetworkAddress &address) {
    int i;
    for (i = 0; i < m_discoveredSessions.size(); i++) {
        if (m_discoveredSessions[i].address == address.host().toString() && m_discoveredSessions[i].port == moduleInformation.port)
            break;
    }

    if (i == m_discoveredSessions.size())
        return;

    int row = m_savedSessions.size() + i;
    beginRemoveRows(QModelIndex(), row, row);
    m_discoveredSessions.removeAt(i);
    endInsertRows();
}

int QnLoginSessionsModel::savedSessionIndex(int row) const {
    if (row < m_savedSessions.size())
        return row;
    return -1;
}

int QnLoginSessionsModel::discoveredSessionIndex(int row) const {
    if (row >= m_savedSessions.size() && row < rowCount())
        return row - m_savedSessions.size();
    return -1;
}

QnLoginSession QnLoginSessionsModel::session(int row) const {
    int i = savedSessionIndex(row);
    if (i >= 0)
        return m_savedSessions[i];

    i = discoveredSessionIndex(row);
    if (i >= 0)
        return m_discoveredSessions[i];

    return QnLoginSession();
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

QString QnLoginSession::id() const {
    return address + QString::number(port) + user;
}
