#include "context_settings.h"

#include "mobile_client/mobile_client_settings.h"
#include "session_settings.h"

class QnContextSettingsPrivate : public QObject {
    Q_DECLARE_PUBLIC(QnContextSettings)
    QnContextSettings *q_ptr;

public:
    QnContextSettingsPrivate(QnContextSettings *parent);

    void at_valueChanged(int valueId);
    void at_sessionSettings_valueChanged(int valueId);

    void setSessionId(const QString &id);

    void loadSession();
    void saveSession();

    QString sessionId;
    QnSessionSettings *sessionSettings;
};


QnContextSettingsPrivate::QnContextSettingsPrivate(QnContextSettings *parent)
    : q_ptr(parent)
    , sessionSettings(nullptr)
{
    connect(qnSettings, &QnMobileClientSettings::valueChanged, this, &QnContextSettingsPrivate::at_valueChanged);
}

void QnContextSettingsPrivate::at_valueChanged(int valueId) {
    Q_Q(QnContextSettings);

    switch (valueId) {
    case QnMobileClientSettings::ShowOfflineCameras:
        q->showOfflineCamerasChanged();
        break;
    default:
        break;
    }
}

void QnContextSettingsPrivate::at_sessionSettings_valueChanged(int valueId) {
    Q_Q(QnContextSettings);

    switch (valueId) {
    case QnSessionSettings::HiddenCameras:
        q->hiddenCamerasChanged();
        break;
    case QnSessionSettings::HiddenCamerasCollapsed:
        q->hiddenCamerasCollapsedChanged();
        break;
    default:
        break;
    }
}

void QnContextSettingsPrivate::setSessionId(const QString &id) {
    if (sessionId == id)
        return;

    sessionId = id;

    if (sessionSettings) {
        disconnect(sessionSettings, nullptr, this, nullptr);
        sessionSettings->deleteLater();
    }

    sessionSettings = new QnSessionSettings(this, id);
    connect(sessionSettings, &QnSessionSettings::valueChanged, this, &QnContextSettingsPrivate::at_sessionSettings_valueChanged);

    Q_Q(QnContextSettings);
    q->hiddenCamerasChanged();
    q->hiddenCamerasCollapsedChanged();
}


QnContextSettings::QnContextSettings(QObject *parent) :
    QObject(parent),
    d_ptr(new QnContextSettingsPrivate(this))
{
}

QnContextSettings::~QnContextSettings() {
}

bool QnContextSettings::showOfflineCameras() const {
    return qnSettings->showOfflineCameras();
}

void QnContextSettings::setShowOfflineCameras(bool showOfflineCameras) {
    qnSettings->setShowOfflineCameras(showOfflineCameras);
}

QString QnContextSettings::sessionId() const {
    Q_D(const QnContextSettings);
    return d->sessionId;
}

void QnContextSettings::setSessionId(const QString &sessionId) {
    Q_D(QnContextSettings);
    d->setSessionId(sessionId);
}

QStringList QnContextSettings::hiddenCameras() const {
    Q_D(const QnContextSettings);

    if (!d->sessionSettings)
        return QStringList();

    return d->sessionSettings->hiddenCameras().toList();
}

void QnContextSettings::setHiddenCameras(const QStringList &hiddenCameras) {
    Q_D(QnContextSettings);

    if (!d->sessionSettings)
        return;

    d->sessionSettings->setHiddenCameras(QnStringSet::fromList(hiddenCameras));
}

bool QnContextSettings::hiddenCamerasCollapsed() const {
    Q_D(const QnContextSettings);

    if (!d->sessionSettings)
        return false;

    return d->sessionSettings->isHiddenCamerasCollapsed();
}

void QnContextSettings::setHiddenCamerasCollapsed(bool collapsed) {
    Q_D(QnContextSettings);

    if (!d->sessionSettings)
        return;

    d->sessionSettings->setHiddenCamerasCollapsed(collapsed);
}
