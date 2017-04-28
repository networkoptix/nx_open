#include "log.h"
#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>

#include <QtCore/QThread>
#include <QtCore/QDateTime>

#include "../string.h"

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    #include <sys/types.h>
    #include <linux/unistd.h>
    static pid_t gettid(void) { return syscall(__NR_gettid); }
#endif

const char *qn_logLevelNames[] = {"UNKNOWN", "NONE", "ALWAYS", "ERROR", "WARNING", "INFO", "DEBUG", "DEBUG2"};

QnLogLevel QnLog::logLevelFromString(const QString &value) {
    QString str = value.toUpper().trimmed();
    if (str == QLatin1String("DEBUG1")) str = QLatin1String("DEBUG");

    for (uint i = 0; i < sizeof(qn_logLevelNames)/sizeof(char*); ++i) {
        if (str == QLatin1String(qn_logLevelNames[i]))
            return QnLogLevel(i);
    }

    return cl_logUNKNOWN;
}

QString QnLog::logLevelToString(QnLogLevel value) {
    return QLatin1String(qn_logLevelNames[value]);
}

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

void QnLog::writeToStdout(const QString& str, QnLogLevel logLevel)
{
    switch (logLevel)
    {
        case cl_logERROR:
        case cl_logWARNING:
            std::cerr << str.toStdString() << std::endl;
            break;

        default:
            std::cout << str.toStdString() << std::endl;
            break;
    }
}

#endif // !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

// -------------------------------------------------------------------------- //
// QnLogPrivate
// -------------------------------------------------------------------------- //
class QnLogPrivate {
public:
    QnLogPrivate():
        m_logLevel(static_cast<QnLogLevel>(0)) /* Log nothing by default. */
    {}

    ~QnLogPrivate() {
        m_file.close();
    }

    bool create(const QString& baseName, quint32 maxFileSize, quint8 maxBackupFiles, QnLogLevel logLevel)
    {
        m_baseName = baseName;
        m_maxFileSize = maxFileSize;
        m_maxBackupFiles = maxBackupFiles;
        m_curNum = 0;

        m_logLevel = logLevel;
        openFileImpl();
        return !m_file.fail();
    }

    void setLogLevel(QnLogLevel logLevel)
    {
        m_logLevel = logLevel;
    }

    QnLogLevel logLevel() const
    {
        return m_logLevel;
    }

    void log(const QString& msg, QnLogLevel logLevel)
    {
        if (logLevel > m_logLevel)
            return;

        static constexpr int kPreambuleMaxLength = 48;

        QString str;
        str.reserve(msg.size() + kPreambuleMaxLength);
        str += QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz ");
        #if defined(Q_OS_LINUX)
            str += lit(" %1").arg(gettid(), 6);
        #else
            str += lit(" %1").arg((qint64) QThread::currentThreadId(), 6, 16);
        #endif
        str += lit(" %1: ").arg(qn_logLevelNames[logLevel], 7);
        str += msg;

        std::unique_lock<std::mutex> lk(m_mutex);

        if (m_baseName == lit("-") || m_file.fail())
        {
            QnLog::writeToStdout(str, logLevel);
            return;
        }

        m_file << str.toStdString() << std::endl;
        m_file.flush();
        if (m_file.tellp() >= m_maxFileSize)
            openNextFile();
    }

    QString syncCurrFileName() const
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        return currFileName();
    }

private:
    void openNextFile()
    {
        m_file.close();

        int max_existing_num = m_maxBackupFiles;
        while (max_existing_num)
        {
            if (QFile::exists(backupFileName(max_existing_num)))
                break;
            --max_existing_num;
        }

        if (max_existing_num < m_maxBackupFiles) // if we do not need to delete backupfiles
        {
            QFile::rename(currFileName(), backupFileName(max_existing_num+1));
        }
        else // we need to delete one backup file
        {
            QFile::remove(backupFileName(1)); // delete the oldest file

            for (int i = 2; i <= m_maxBackupFiles; ++i) // shift all file names by one
                QFile::rename(backupFileName(i), backupFileName(i-1));

            QFile::rename(currFileName(), backupFileName(m_maxBackupFiles)); // move current to the most latest backup
        }

        openFileImpl();
    }

    void openFileImpl()
    {
        if (m_file.is_open())
            return;

        #ifdef Q_OS_WIN
            m_file.open(currFileName().toStdWString(), std::ios_base::in | std::ios_base::out);
        #else
            m_file.open(currFileName().toStdString(), std::ios_base::in | std::ios_base::out);
        #endif

        if (!m_file.fail())
        {
            // File exists, prepare to append.
            m_file.seekp(std::ios_base::streamoff(0), std::ios_base::end);
            return;
        }

        #ifdef Q_OS_WIN
            m_file.open(currFileName().toStdWString(), std::ios_base::out);
        #else
            m_file.open(currFileName().toStdString(), std::ios_base::out);
        #endif

        if (m_file.fail())
            return;

        // Ensure 1st char is UTF8 BOM.
        m_file.write("\xEF\xBB\xBF", 3);
    }

    QString currFileName() const
    {
        return m_baseName + QLatin1String(".log");
    }

    QString backupFileName(quint8 num) const
    {
        QString result;
        QTextStream stream(&result);

        char cnum[4];

#ifdef Q_CC_MSVC
        sprintf_s(cnum, 4, "%.3d", num);
#else
        snprintf(cnum, 4, "%.3d", num);
#endif

        stream << m_baseName << cnum << ".log";
        return result;
    }

private:
    quint32 m_maxFileSize;
    quint8 m_maxBackupFiles;
    quint8 m_curNum;
    QString m_baseName;
    QnLogLevel m_logLevel;
    std::fstream m_file;
    mutable std::mutex m_mutex;
};


// -------------------------------------------------------------------------- //
// QnLog
// -------------------------------------------------------------------------- //

class QnLogs
{
public:
    ~QnLogs()
    {
        //no one calls get() anymore
        for( auto val: m_logs )
            delete val.second;
        m_logs.clear();
    }

    QnLog* get( int logID )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        QnLog*& log = m_logs[logID];
        if( !log )
            log = new QnLog();
        return log;
    }

    bool exists( int logID )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        return m_logs.find(logID) != m_logs.cend();
    }

    bool put( int logID, QnLog* log )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        return m_logs.insert( std::make_pair( logID, log ) ).second;
    }

private:
    std::map<int, QnLog*> m_logs;
    std::mutex m_mutex;
};

bool QnLog::s_disableLogConfiguration(false);

QnLog::QnLog()
:
    d( new QnLogPrivate() )
{
}

QnLog::~QnLog()
{
    delete d;
}

const std::shared_ptr< QnLog::Logs >& QnLog::logs()
{
    std::call_once( s_onceFlag, [](){ s_instance.reset( new Logs ); } );
    return s_instance;
}

const std::unique_ptr< QnLog >& QnLog::instance( int logID )
{
    return logs()->get( logID );
}

bool QnLog::create(const QString& baseName, quint32 maxFileSize, quint8 maxBackupFiles, QnLogLevel logLevel) {
    if(!d)
        return false;

    if(s_disableLogConfiguration)
        return true;

    return d->create(baseName, maxFileSize, maxBackupFiles, logLevel);
}

void QnLog::setLogLevel(QnLogLevel logLevel) {
    if(!d)
        return;

    d->setLogLevel(logLevel);
}

QnLogLevel QnLog::logLevel() const {
    if(!d)
        return cl_logERROR;

    return d->logLevel();
}

void QnLog::log(const QString& msg, QnLogLevel logLevel) {
    if(!d)
        return;

    d->log(msg, logLevel);
}

void QnLog::log(QnLogLevel logLevel, const char* format, ...) {
    if(!d)
        return;

    if (logLevel > d->logLevel())
        return;

    static const int MAX_MESSAGE_SIZE = 1024;

    char buffer[MAX_MESSAGE_SIZE];
    va_list args;

    va_start(args, format);
#ifdef Q_CC_MSVC
    vsnprintf_s(buffer, MAX_MESSAGE_SIZE, MAX_MESSAGE_SIZE, format, args);
#else
    vsnprintf(buffer, MAX_MESSAGE_SIZE, format, args);
#endif

    d->log(QString::fromLatin1(buffer), logLevel);
    va_end(args);
}

void qnLogMsgHandler(QtMsgType type, const QMessageLogContext& /*ctx*/, const QString& msg) {
    //TODO: #Elric use ctx

    QnLogLevel logLevel;
    switch (type) {
    case QtFatalMsg:
    case QtCriticalMsg:
        logLevel = cl_logERROR;
        break;
    case QtWarningMsg:
        logLevel = cl_logWARNING;
        break;
    case QtDebugMsg:
        logLevel = cl_logDEBUG1;
        break;
    default:
        logLevel = cl_logINFO;
        break;
    }

    NX_LOG(msg, logLevel);
}

void QnLog::initLog(const QString &logLevelStr, int logID )
{
    if (s_disableLogConfiguration)
        return;

    logs()->init( logID, logLevelStr );
}

bool QnLog::initLog(QnLog *externalInstance, int logID )
{
    if (s_disableLogConfiguration)
        return true;

    return logs()->init( logID, std::unique_ptr< QnLog >( externalInstance ) );
}

void QnLog::applyArguments(const nx::utils::ArgumentParser& arguments)
{
    QnLogLevel logLevel(cl_logNONE);

    std::uint64_t maxFileSize = 1024 * 1024 * 10;
    if (const auto value = arguments.get("log-size", "ls"))
        maxFileSize = nx::utils::stringToBytes(*value);

    if (const auto value = arguments.get("log-file", "lf"))
    {
        logLevel = cl_logDEBUG1;
        instance()->create(*value, (quint32)maxFileSize, 5, logLevel);
    }

    if (const auto value = arguments.get("log-level", "ll"))
    {
        logLevel = logLevelFromString(*value);
        NX_CRITICAL(logLevel != cl_logUNKNOWN);
    }

    instance()->setLogLevel(logLevel);
    s_disableLogConfiguration = true;
}

QString QnLog::logFileName( int logID )
{
    return instance(logID)->d->syncCurrFileName();
}

const std::unique_ptr< QnLog >& QnLog::Logs::get( int logID )
{
    std::unique_lock<std::mutex> lk( m_mutex );
    //TODO #ak aren't we supposed to explcitly create log with init call?
    auto& log = m_logs[logID];
    if( !log )
        log.reset( new QnLog );
    return log;
}

bool QnLog::Logs::exists(int logID) const
{
    std::unique_lock<std::mutex> lk(m_mutex);
    return m_logs.find(logID) != m_logs.end();
}

bool QnLog::Logs::init( int logID, std::unique_ptr< QnLog > log )
{
    std::unique_lock<std::mutex> lk( m_mutex );
    return m_logs.emplace( logID, std::move( log ) ).second;
}

bool QnLog::Logs::init( int logID, const QString& logLevelStr )
{
    bool needWarnLogLevel = false;
    QnLogLevel logLevel = cl_logDEBUG1;
#ifndef _DEBUG
    logLevel = cl_logINFO;
#endif
    if (!logLevelStr.isEmpty())
    {
        QnLogLevel newLogLevel = QnLog::logLevelFromString(logLevelStr);
        if (newLogLevel != cl_logUNKNOWN)
            logLevel = newLogLevel;
        else
            needWarnLogLevel = true;
    }

    get(logID)->setLogLevel(logLevel);
    if (needWarnLogLevel) {
        NX_LOG(QLatin1String("================================================================================="), cl_logALWAYS);
        NX_LOG(QString::fromLatin1("Unknown log level specified. Using level %1").arg(QnLog::logLevelToString(logLevel)), cl_logALWAYS);
    }
    return true;
}

std::once_flag QnLog::s_onceFlag;
std::shared_ptr< QnLog::Logs > QnLog::s_instance;
