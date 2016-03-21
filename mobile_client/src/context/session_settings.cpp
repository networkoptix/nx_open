#include "session_settings.h"

#include <QtCore/QStandardPaths>
#include <QtCore/QSettings>

#include "utils/common/scoped_value_rollback.h"

namespace {
    const QString sessionsDir = lit("sessions");
}

QnSessionSettings::QnSessionSettings(QObject *parent, const QString &sessionId)
    : base_type(parent)
    , m_settings(nullptr)
    , m_loading(true)
{
    QDir configDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    configDir.mkdir(sessionsDir);
    configDir.cd(sessionsDir);
    QString sessionConfigFile = configDir.absoluteFilePath(sessionId);
    m_settings = new QSettings(sessionConfigFile, QSettings::IniFormat, this);

    init();
    updateFromSettings(m_settings);
    setThreadSafe(true);
    m_loading = false;
}

QString QnSessionSettings::settingsFileName(const QString &sessionId) {
    QDir configDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    return configDir.absoluteFilePath(sessionsDir + lit("/") + sessionId);
}

void QnSessionSettings::updateValuesFromSettings(QSettings *settings, const QList<int> &ids) {
    QN_SCOPED_VALUE_ROLLBACK(&m_loading, true);
    base_type::updateValuesFromSettings(settings, ids);
}

QnPropertyStorage::UpdateStatus QnSessionSettings::updateValue(int id, const QVariant &value) {
    UpdateStatus status = base_type::updateValue(id, value);

    /* Settings are to be written out right away. */
    if(!m_loading && status == Changed)
        writeValueToSettings(m_settings, id, this->value(id));

    return status;
}
