#include "settings.h"

#include "command_line_parser.h"
#include "app_info.h"

QnSettings::QnSettings(
    const QString& applicationName,
    const QString& moduleName,
    QSettings::Scope scope)
    :
    m_applicationName(applicationName),
    m_moduleName(moduleName),
    #ifdef _WIN32
        m_systemSettings(scope, QnAppInfo::organizationName(), applicationName)
    #else
        m_systemSettings(lm("/opt/%1/%2/etc/%2.conf")
            .arg(QnAppInfo::linuxOrganizationName()).arg(moduleName), QSettings::IniFormat)
    #endif
{
    (void) scope; //< Unused on certain platforms.
}

void QnSettings::parseArgs(int argc, const char* argv[])
{
    m_args.parse(argc, argv);
}

bool QnSettings::contains(const QString& key) const
{
    return static_cast<bool>(m_args.get(key))
        || m_systemSettings.contains(key);
}

QVariant QnSettings::value(
    const QString& key,
    const QVariant& defaultValue) const
{
    if (const auto value = m_args.get(key))
        return QVariant(*value);

    return m_systemSettings.value(key, defaultValue);
}

void QnLogSettings::load(const QnSettings& settings, const QString& prefix)
{
    const auto makeKey =
        [&prefix](const char* key)
        {
            return QString(lm("%1/%2").arg(prefix).arg(key));
        };

    const auto confLevel = QnLog::logLevelFromString(settings.value(makeKey("logLevel")).toString());
    if (confLevel != cl_logUNKNOWN)
        level = confLevel;

    directory = settings.value(makeKey("logDir")).toString();
    maxBackupCount = (quint8) settings.value(makeKey("maxBackupCount"), 5).toInt();
    maxFileSize = (quint32) nx::utils::stringToBytes(
        settings.value(makeKey("maxFileSize")).toString(), maxFileSize);
}

void initializeQnLog(
    const QnLogSettings& settings,
    const QString& dataDir,
    const QString& applicationName,
    const QString& baseName,
    int id)
{
    if (settings.level == cl_logNONE)
        return;

    const auto logDir = settings.directory.isEmpty()
        ? (dataDir + QLatin1String("/log"))
        : settings.directory;

    QDir().mkpath(logDir);
    const QString& fileName = logDir + lit("/") + baseName;
    if (!QnLog::instance(id)->create(
        fileName, settings.maxFileSize, settings.maxBackupCount, settings.level))
    {
        std::cerr << "Failed to create log file " << fileName.toStdString() << std::endl;
    }

    const auto logInfo = lm("Logging level: %1, maxFileSize: %2, maxBackupCount: %3, fileName: %4")
        .strs(QnLog::logLevelToString(settings.level),
            nx::utils::bytesToString(settings.maxFileSize),
            settings.maxBackupCount, fileName);

    NX_LOG(id, lm(QByteArray(80, '=')), cl_logALWAYS);
    NX_LOG(id, lm("%1 started").arg(applicationName), cl_logALWAYS);
    NX_LOG(id, lm("Software version: %1").arg(QnAppInfo::applicationVersion()), cl_logALWAYS);
    NX_LOG(id, lm("Software revision: %1").arg(QnAppInfo::applicationRevision()), cl_logALWAYS);
    NX_LOG(id, logInfo, cl_logALWAYS);
}
