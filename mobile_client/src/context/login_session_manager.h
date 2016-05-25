#pragma once

#include <QtCore/QObject>

#include "login_session.h"

class QnLoginSessionManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString lastUsedSessionId READ lastUsedSessionId WRITE setLastUsedSessionId NOTIFY lastUsedSessionIdChanged)

public:
    QnLoginSessionManager(QObject *parent = nullptr);

    QString lastUsedSessionId() const;
    void setLastUsedSessionId(const QString &sessionId);

    QnLoginSession lastUsedSession() const;

    Q_INVOKABLE QUrl lastUsedSessionUrl() const;
    Q_INVOKABLE QString lastUsedSessionHost() const;
    Q_INVOKABLE int lastUsedSessionPort() const;
    Q_INVOKABLE QString lastUsedSessionLogin() const;
    Q_INVOKABLE QString lastUsedSessionPassword() const;
    Q_INVOKABLE QString lastUsedSessionSystemName() const;

signals:
    void lastUsedSessionIdChanged();
};
