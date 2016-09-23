#include "settings.h"

#include "command_line_parser.h"
#include "app_info.h"

QnSettings::QnSettings(const QString& applicationName_, const QString& moduleName_):
    applicationName(applicationName_),
    moduleName(moduleName_),
    #ifdef _WIN32
        m_systemSettings(
            QSettings::SystemScope,
            QnAppInfo::organizationName(),
            applicationName)
    #else
        m_systemSettings(
            lm("/opt/%1/%2/etc/%2.conf").arg(QnAppInfo::linuxOrganizationName()).arg(moduleName),
            QSettings::IniFormat)
    #endif
{
}

void QnSettings::parseArgs(int argc, const char* argv[])
{
    m_args.parse(argc, argv);
}

QVariant QnSettings::value(
    const QString& key,
    const QVariant& defaultValue) const
{
    if (const auto value = m_args.get(key))
        return QVariant(*value);

    return m_systemSettings.value(key, defaultValue);
}

void QnLogSettings::load(const QnSettings& settings, QString prefix)
{
    const auto key = [&prefix](const char* key)
    {
        return QString(lm("%1/%2").arg(prefix).arg(key));
    };

    m_level = QnLog::logLevelFromString(settings.value(key("logLevel")).toString());
    if (m_level == cl_logUNKNOWN)
    {
        #ifdef _DEBUG
            m_level = cl_logDEBUG1;
        #else
            level = cl_logINFO;
        #endif
    }

    m_dir = settings.value(key("logDir")).toString();
    m_maxBackupCount = (quint8) settings.value(key("maxBackupCount"), 5).toInt();
    m_maxFileSize = (quint32) nx::utils::stringToBytes(
            settings.value(key("maxFileSize")).toString(), QLatin1String("10M"));
}

void QnLogSettings::initLog(
    const QString& dataDir,
    const QString& applicationName,
    const QString& baseName,
    int id) const
{
    if (m_level == cl_logNONE)
        return;

    const auto logDir = m_dir.isEmpty() ? (dataDir + QLatin1String("/log")) : m_dir;
    QDir().mkpath(logDir);

    const QString& fileName = logDir + lit("/") + baseName;
    if (!QnLog::instance(id)->create(fileName, m_maxFileSize, m_maxBackupCount, m_level))
    {
        std::wcerr << L"Failed to create log file " << fileName.toStdWString() << std::endl;
    }

    NX_LOG(id, lm(QByteArray(80, '=')), cl_logALWAYS);
    NX_LOG(id, lm("%1 started").arg(applicationName), cl_logALWAYS);
    NX_LOG(id, lm("Software version: %1").arg(QnAppInfo::applicationVersion()), cl_logALWAYS);
    NX_LOG(id, lm("Software revision: %1").arg(QnAppInfo::applicationRevision()), cl_logALWAYS);
    NX_LOG(id, lm("Logging level: %1, maxFileSize: %2, maxBackupCount: %3, fileName: %4")
        .strs(QnLog::logLevelToString(m_level), nx::utils::bytesToString(m_maxFileSize),
            m_maxBackupCount, fileName), cl_logALWAYS);
}
