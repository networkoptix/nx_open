#pragma once

#include <QtCore/QMultiHash>

#include "login_sessions_model.h"
#include <context/login_session.h>
#include <nx/network/socket_common.h>

class QnSavedSessionsModel : public QnLoginSessionsModel
{
    Q_OBJECT

public:
    explicit QnSavedSessionsModel(QObject *parent = nullptr);
    ~QnSavedSessionsModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    const QnLoginSession *session(int row) const override;

public slots:
    QString updateSession(
            const QString &sessionId,
            const QString &address,
            const int port,
            const QString &user,
            const QString &password,
            const QString &systemName,
            bool moveTop = false);
    QString updateSession(
            const QString &sessionId,
            const QUrl &url,
            const QString &systemName,
            bool moveTop = false);
    void deleteSession(const QString &id);

private:
    void loadFromSettings();
    void saveToSettings();

private:
    QList<QnLoginSession> m_savedSessions;
    QMultiHash<SocketAddress, QString> m_sessionIdByAddress;
};
