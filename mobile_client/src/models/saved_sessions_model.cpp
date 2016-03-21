#include "saved_sessions_model.h"

#include <algorithm>

#include <network/module_finder.h>
#include <mobile_client/mobile_client_settings.h>
#include <context/session_settings.h>

QnSavedSessionsModel::QnSavedSessionsModel(QObject *parent)
    : QnLoginSessionsModel(parent)
{
    loadFromSettings();

    if (QnModuleFinder *moduleFinder = QnModuleFinder::instance()) {
        auto updateModuleWithAddress = [this](const QnModuleInformation &moduleInformation, const SocketAddress &address) {
            for (QString id: m_sessionIdByAddress.values(address)) {
                auto it = std::find_if(m_savedSessions.begin(), m_savedSessions.end(), [&id](const QnLoginSession &session) -> bool {
                    return session.id == id;
                });

                if (it == m_savedSessions.end())
                    continue;

                it->systemName = moduleInformation.systemName;
                QModelIndex idx = index(std::distance(m_savedSessions.begin(), it));
                emit dataChanged(idx, idx);
            }
        };

        connect(moduleFinder, &QnModuleFinder::moduleAddressFound, this, updateModuleWithAddress);
        connect(moduleFinder, &QnModuleFinder::moduleChanged, this, [this, moduleFinder, updateModuleWithAddress](const QnModuleInformation &moduleInformation) {
            for (const SocketAddress &address: moduleFinder->moduleAddresses(moduleInformation.id)) {
                for (QString id: m_sessionIdByAddress.values(address))
                    updateModuleWithAddress(moduleInformation, address);
            }
        });
    }
}

QnSavedSessionsModel::~QnSavedSessionsModel()
{
}

int QnSavedSessionsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_savedSessions.size();
}

const QnLoginSession *QnSavedSessionsModel::session(int row) const
{
    return &m_savedSessions[row];
}

QString QnSavedSessionsModel::updateSession(
        const QString &sessionId,
        const QString &address,
        const int port,
        const QString &user,
        const QString &password,
        const QString &systemName,
        bool moveTop)
{
    auto exactSessionPredicate = [&sessionId](const QnLoginSession &session) -> bool {
        return session.id == sessionId;
    };

    auto similarSessionPredicate = [&address, &port, &user](const QnLoginSession &session) -> bool {
        return session.address == address &&
               session.port == port &&
               session.user == user;
    };

    auto it = sessionId.isNull()
              ? std::find_if(m_savedSessions.begin(), m_savedSessions.end(), similarSessionPredicate)
              : std::find_if(m_savedSessions.begin(), m_savedSessions.end(), exactSessionPredicate);

    QString savedId;

    if (it != m_savedSessions.end()) {
        m_sessionIdByAddress.remove(SocketAddress(it->address, it->port), it->id);

        it->address = address;
        it->port = port;
        it->user = user;
        it->password = password;
        it->systemName = systemName;

        savedId = it->id;

        int row = std::distance(m_savedSessions.begin(), it);
        if (moveTop && row > 0) {
            beginMoveRows(QModelIndex(), row, row, QModelIndex(), 0);
            m_savedSessions.move(row, 0);
            endMoveRows();
            row = 0;
        }

        if (row >= 0) {
            QModelIndex index = this->index(row);
            emit dataChanged(index, index);
        }

        m_sessionIdByAddress.insert(SocketAddress(address, port), it->id);
    } else {
        QnLoginSession session;
        session.systemName = systemName;
        session.address = address;
        session.port = port;
        session.user = user;
        session.password = password;

        savedId = session.id;

        beginInsertRows(QModelIndex(), 0, 0);
        m_savedSessions.insert(0, session);
        endInsertRows();

        m_sessionIdByAddress.insert(SocketAddress(session.address, session.port), session.id);
    }

    saveToSettings();

    return savedId;
}

void QnSavedSessionsModel::deleteSession(const QString &id) {
    auto it = std::find_if(m_savedSessions.begin(), m_savedSessions.end(), [&id](const QnLoginSession &session) {
        return session.id == id;
    });

    if (it == m_savedSessions.end())
        return;

    m_sessionIdByAddress.remove(SocketAddress(it->address, it->port), it->id);

    int row = std::distance(m_savedSessions.begin(), it);
    beginRemoveRows(QModelIndex(), row, row);
    m_savedSessions.erase(it);
    endRemoveRows();

    saveToSettings();
    QFile::remove(QnSessionSettings::settingsFileName(id));
}

void QnSavedSessionsModel::loadFromSettings() {
    QVariantList variantList = qnSettings->savedSessions();

    beginResetModel();

    m_savedSessions.clear();
    m_sessionIdByAddress.clear();

    for (const QVariant &variant: variantList) {
        QnLoginSession session = QnLoginSession::fromVariant(variant.toMap());
        if (session.port == -1)
            continue;

        m_savedSessions.append(session);
        m_sessionIdByAddress.insert(SocketAddress(session.address, session.port), session.id);
    }

    endResetModel();
}

void QnSavedSessionsModel::saveToSettings() {
    QVariantList variantList;
    for (const QnLoginSession &session: m_savedSessions)
        variantList.append(session.toVariant());
    qnSettings->setSavedSessions(variantList);
}
