#include "login_session_manager.h"

#include "login_session.h"
#include "mobile_client/mobile_client_settings.h"

QnLoginSessionManager::QnLoginSessionManager(QObject *parent) :
    QObject(parent)
{
}

QString QnLoginSessionManager::lastUsedSessionId() const {
    QString id = qnSettings->lastUsedSessionId();
    if (id.isEmpty())
        return QString();

    QVariantList sessions = qnSettings->savedSessions();
    if (sessions.isEmpty())
        return QString();

    QnLoginSession session = QnLoginSession::fromVariant(sessions.first().toMap());
    if (session.id != id)
        return QString();

    return id;
}

void QnLoginSessionManager::setLastUsedSessionId(const QString &sessionId) {
    QString id = qnSettings->lastUsedSessionId();
    if (id == sessionId)
        return;

    qnSettings->setLastUsedSessionId(sessionId);
    emit lastUsedSessionIdChanged();
}

QnLoginSession QnLoginSessionManager::lastUsedSession() const {
    if (lastUsedSessionId().isEmpty())
        return QnLoginSession();

    return QnLoginSession::fromVariant(qnSettings->savedSessions().first().toMap());
}

QUrl QnLoginSessionManager::lastUsedSessionUrl() const {
    return lastUsedSession().url();
}

QString QnLoginSessionManager::lastUsedSessionHost() const {
    return lastUsedSession().address;
}

int QnLoginSessionManager::lastUsedSessionPort() const {
    return lastUsedSession().port;
}

QString QnLoginSessionManager::lastUsedSessionLogin() const {
    return lastUsedSession().user;
}

QString QnLoginSessionManager::lastUsedSessionPassword() const {
    return lastUsedSession().password;
}

QString QnLoginSessionManager::lastUsedSessionSystemName() const {
    return lastUsedSession().systemName;
}

