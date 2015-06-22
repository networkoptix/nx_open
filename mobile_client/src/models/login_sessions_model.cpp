#include "login_sessions_model.h"

#include <algorithm>

#include "mobile_client/mobile_client_settings.h"
#include "utils/network/module_finder.h"

QnLoginSessionsModel::QnLoginSessionsModel(QObject *parent) :
    QAbstractListModel(parent),
    m_displayMode(ShowAll),
    m_moduleFinder(QnModuleFinder::instance())
{
    loadFromSettings();

    connect(m_moduleFinder,     &QnModuleFinder::moduleAddressFound,    this,   &QnLoginSessionsModel::at_moduleFinder_moduleAddressFound);
    connect(m_moduleFinder,     &QnModuleFinder::moduleAddressLost,     this,   &QnLoginSessionsModel::at_moduleFinder_moduleAddressLost);

    for (const QnModuleInformationWithAddresses &moduleInformation : m_moduleFinder->foundModulesWithAddresses()) {
        for (const QString &address : moduleInformation.remoteAddresses) {
            at_moduleFinder_moduleAddressFound(moduleInformation, SocketAddress(address, moduleInformation.port));
        }
    }
}

QnLoginSessionsModel::~QnLoginSessionsModel() {
}

int QnLoginSessionsModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)

    int result = 0;
    if (m_displayMode.testFlag(ShowSaved))
        result += m_savedSessions.size();
    if (m_displayMode.testFlag(ShowDiscovered))
        result += m_discoveredSessions.size();

    return result;
}

QVariant QnLoginSessionsModel::data(const QModelIndex &index, int role) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnLoginSession &session = this->session(index.row());

    switch (role) {
    case SessionIdRole:
        return session.sessionId;
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

QnLoginSessionsModel::DisplayModeFlags QnLoginSessionsModel::displayMode() const {
    return m_displayMode;
}

void QnLoginSessionsModel::setDisplayMode(DisplayModeFlags displayMode) {
    if (m_displayMode == displayMode)
        return;

    beginResetModel();
    m_displayMode = displayMode;
    endResetModel();
}

QString QnLoginSessionsModel::updateSession(const QString &sessionId, const QString &address, const int port, const QString &user, const QString &password, const QString &systemName) {
    auto predicate = [&sessionId](const QnLoginSession &session) -> bool {
        return session.sessionId == sessionId;
    };

    auto it = std::find_if(m_savedSessions.begin(), m_savedSessions.end(), predicate);
    if (it != m_savedSessions.end()) {
        it->address = address;
        it->port = port;
        it->user = user;
        it->password = password;
        it->systemName = systemName;

        int row = savedSessionRow(it - m_savedSessions.begin());
        if (row == 0) {
            QModelIndex index = this->index(0);
            emit dataChanged(index, index);
        } else if (row > 0) {
            beginMoveRows(QModelIndex(), row, row, QModelIndex(), 0);
            m_savedSessions.move(row, 0);
            endInsertRows();
        }
    } else {
        QnLoginSession session;
        session.systemName = systemName;
        session.address = address;
        session.port = port;
        session.user = user;
        session.password = password;

        int row = savedSessionRow(0);
        if (row >= 0) {
            beginInsertRows(QModelIndex(), 0, 0);
            m_savedSessions.insert(0, session);
            endInsertRows();
        } else {
            m_savedSessions.insert(0, session);
        }
    }

    saveToSettings();

    return m_savedSessions[0].sessionId;
}

void QnLoginSessionsModel::deleteSession(const QString &id) {
    for (int i = 0; i < m_savedSessions.size(); i++) {
        if (m_savedSessions[i].sessionId != id)
            continue;

        int row = savedSessionRow(i);
        if (row >= 0) {
            beginRemoveRows(QModelIndex(), i, i);
            m_savedSessions.removeAt(i);
            endRemoveRows();
        } else {
            m_savedSessions.removeAt(i);
        }
    }

    saveToSettings();
}

void QnLoginSessionsModel::at_moduleFinder_moduleAddressFound(const QnModuleInformation &moduleInformation, const SocketAddress &address) {
    bool found = false;
    for (const QnLoginSession &session: m_discoveredSessions) {
        if (session.address == address.address.toString() && session.port == address.port) {
            found = true;
            break;
        }
    }

    if (found)
        return;

    QnLoginSession session;
    session.address = address.address.toString();
    session.port = address.port;
    session.systemName = moduleInformation.systemName;

    int row = discoveredSessionRow(m_discoveredSessions.size());
    if (row >= 0) {
        beginInsertRows(QModelIndex(), row, row);
        m_discoveredSessions.append(session);
        endInsertRows();
    } else {
        m_discoveredSessions.append(session);
    }
}

void QnLoginSessionsModel::at_moduleFinder_moduleAddressLost(const QnModuleInformation &moduleInformation, const SocketAddress &address) {
    int i;
    for (i = 0; i < m_discoveredSessions.size(); i++) {
        if (m_discoveredSessions[i].address == address.address.toString() && m_discoveredSessions[i].port == moduleInformation.port)
            break;
    }

    if (i == m_discoveredSessions.size())
        return;

    int row = discoveredSessionRow(i);
    if (row >= 0) {
        beginRemoveRows(QModelIndex(), row, row);
        m_discoveredSessions.removeAt(i);
        endInsertRows();
    } else {
        m_discoveredSessions.removeAt(i);
    }
}

int QnLoginSessionsModel::savedSessionIndex(int row) const {
    if (m_displayMode.testFlag(ShowSaved) && row < m_savedSessions.size())
        return row;

    return -1;
}

int QnLoginSessionsModel::discoveredSessionIndex(int row) const {
    if (!m_displayMode.testFlag(ShowDiscovered))
        return -1;

    int savedCount = m_displayMode.testFlag(ShowSaved) ? m_savedSessions.size() : 0;

    if (row >= savedCount && row < rowCount())
        return row - savedCount;

    return -1;
}

int QnLoginSessionsModel::savedSessionRow(int index) const {
    if (!m_displayMode.testFlag(ShowSaved))
        return -1;

    return index;
}

int QnLoginSessionsModel::discoveredSessionRow(int index) const {
    if (!m_displayMode.testFlag(ShowDiscovered))
        return -1;

    return index + (m_displayMode.testFlag(ShowSaved) ? m_savedSessions.size() : 0);
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

QnLoginSession::QnLoginSession() : port(-1) {
    sessionId = QnUuid::createUuid().toString();
}

QVariantMap QnLoginSession::toVariant() const {
    QVariantMap variant;
    variant[lit("sessionId")] = sessionId;
    variant[lit("systemName")] = systemName;
    variant[lit("address")] = address;
    variant[lit("port")] = port;
    variant[lit("user")] = user;
    variant[lit("password")] = password;
    return variant;
}

QnLoginSession QnLoginSession::fromVariant(const QVariantMap &variant) {
    QnLoginSession session;
    session.sessionId = variant[lit("sessionId")].toString();
    session.systemName = variant[lit("systemName")].toString();
    session.address = variant[lit("address")].toString();
    session.port = variant.value(lit("port"), -1).toInt();
    session.user = variant[lit("user")].toString();
    session.password = variant[lit("password")].toString();
    return session;
}
