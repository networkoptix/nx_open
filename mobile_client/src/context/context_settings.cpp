#include "context_settings.h"

#include "mobile_client/mobile_client_settings.h"
#include "session_settings.h"

class QnContextSettingsPrivate : public QObject
{
    Q_DECLARE_PUBLIC(QnContextSettings)
    QnContextSettings* q_ptr;

public:
    QnContextSettingsPrivate(QnContextSettings* parent);

    void at_valueChanged(int valueId);
    void at_sessionSettings_valueChanged(int valueId);

    void setSystemId(const QString& id);

    void loadSession();
    void saveSession();

    QString systemId;
    QnSessionSettings* sessionSettings;
};


QnContextSettingsPrivate::QnContextSettingsPrivate(QnContextSettings* parent)
    : q_ptr(parent)
    , sessionSettings(nullptr)
{
    connect(qnSettings, &QnMobileClientSettings::valueChanged, this, &QnContextSettingsPrivate::at_valueChanged);
}

void QnContextSettingsPrivate::at_valueChanged(int valueId)
{
    switch (valueId)
    {
    default:
        break;
    }
}

void QnContextSettingsPrivate::at_sessionSettings_valueChanged(int valueId)
{
    switch (valueId)
    {
    default:
        break;
    }
}

void QnContextSettingsPrivate::setSystemId(const QString &id)
{
    if (systemId == id)
        return;

    systemId = id;

    if (sessionSettings)
    {
        disconnect(sessionSettings, nullptr, this, nullptr);
        sessionSettings->deleteLater();
    }

    sessionSettings = new QnSessionSettings(this, id);
    connect(sessionSettings, &QnSessionSettings::valueChanged, this, &QnContextSettingsPrivate::at_sessionSettings_valueChanged);
}


QnContextSettings::QnContextSettings(QObject* parent)
    : QObject(parent)
    , d_ptr(new QnContextSettingsPrivate(this))
{
}

QnContextSettings::~QnContextSettings()
{
}

QString QnContextSettings::systemId() const
{
    Q_D(const QnContextSettings);
    return d->systemId;
}

void QnContextSettings::setSystemId(const QString &sessionId)
{
    Q_D(QnContextSettings);
    d->setSystemId(sessionId);
}
