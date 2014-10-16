#include "context_settings.h"

#include "mobile_client/mobile_client_settings.h"

QnContextSettings::QnContextSettings(QObject *parent) :
    QObject(parent)
{
}

QVariantList QnContextSettings::savedSessions() const {
    return qnSettings->savedSessions();
}

void QnContextSettings::saveSession(const QVariantMap &session, int index) {
    QVariantList sessions = qnSettings->savedSessions();
    if (index < 0 || index >= sessions.size()) {
        sessions.append(session);
    } else {
        sessions.removeAt(index);
        sessions.prepend(session);
    }
    qnSettings->setSavedSessions(sessions);
}
