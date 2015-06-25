#include "context_settings.h"

#include "mobile_client/mobile_client_settings.h"

QnContextSettings::QnContextSettings(QObject *parent) :
    QObject(parent),
    m_hiddenCamerasCollapsed(true)
{
    connect(qnSettings, &QnMobileClientSettings::valueChanged, this, &QnContextSettings::at_valueChanged);
}

bool QnContextSettings::showOfflineCameras() const {
    return qnSettings->showOfflineCameras();
}

void QnContextSettings::setShowOfflineCameras(bool showOfflineCameras) {
    qnSettings->setShowOfflineCameras(showOfflineCameras);
}

QString QnContextSettings::sessionId() const {
    return m_sessionId;
}

void QnContextSettings::setSessionId(const QString &sessionId) {
    if (m_sessionId == sessionId)
        return;

    m_sessionId = sessionId;
    emit sessionIdChanged();

    loadSession();
}

QStringList QnContextSettings::hiddenCameras() const {
    return m_hiddenCameras.toList();
}

void QnContextSettings::setHiddenCameras(const QStringList &hiddenCameras) {
    QSet<QString> hiddenCamerasSet = QSet<QString>::fromList(hiddenCameras);
    if (m_hiddenCameras == hiddenCamerasSet)
        return;

    m_hiddenCameras = hiddenCamerasSet;
    emit hiddenCamerasChanged();

    saveSession();
}

bool QnContextSettings::hiddenCamerasCollapsed() const {
    return m_hiddenCamerasCollapsed;
}

void QnContextSettings::setHiddenCamerasCollapsed(bool collapsed) {
    if (m_hiddenCamerasCollapsed == collapsed)
        return;

    m_hiddenCamerasCollapsed = collapsed;
    emit hiddenCamerasCollapsedChanged();

    saveSession();
}

void QnContextSettings::at_valueChanged(int valueId) {
    switch (valueId) {
    case QnMobileClientSettings::ShowOfflineCameras:
        emit showOfflineCamerasChanged();
        break;
    default:
        break;
    }
}

void QnContextSettings::loadSession() {
    QVariantMap map = qnSettings->sessionSettings().value(m_sessionId).toMap();
    setHiddenCameras(map.value(lit("hiddenCameras")).toStringList());
    setHiddenCamerasCollapsed(map.value(lit("hiddenCamerasCollapsed"), true).toBool());
}

void QnContextSettings::saveSession() {
    if (m_sessionId.isEmpty())
        return;

    QVariantMap sessionSettings = qnSettings->sessionSettings();
    QVariantMap map = sessionSettings.value(m_sessionId).toMap();
    map[lit("hiddenCameras")] = hiddenCameras();
    map[lit("hiddenCamerasCollapsed")] = hiddenCamerasCollapsed();
    sessionSettings[m_sessionId] = map;
    qnSettings->setSessionSettings(sessionSettings);
}
