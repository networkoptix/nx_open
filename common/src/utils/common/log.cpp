#include "log.h"
#include <iomanip>
#include <sstream>
#include <QtCore/QTextStream>
#include <QtCore/QThread>
#include <QtCore/QDateTime>
#include <QtCore/QLocale>

#ifdef Q_OS_LINUX
#   include <sys/types.h>
#   include <linux/unistd.h>
static pid_t gettid(void) { return syscall(__NR_gettid); }
#endif


const char *qn_logLevelNames[] = {"UNKNOWN", "ALWAYS", "ERROR", "WARNING", "INFO", "DEBUG", "DEBUG2"};
const char UTF8_BOM[] = "\xEF\xBB\xBF";

QnLogLevel QnLog::logLevelFromString(const QString &value) {
    QString str = value.toUpper().trimmed();
    for (uint i = 0; i < sizeof(qn_logLevelNames)/sizeof(char*); ++i) {
        if (str == QLatin1String(qn_logLevelNames[i]))
            return QnLogLevel(i);
    }

    return cl_logUNKNOWN;
}

QString QnLog::logLevelToString(QnLogLevel value) {
    return QLatin1String(qn_logLevelNames[value]);
}

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

        m_file.setFileName(currFileName());

        bool rez = m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Unbuffered);
        if (rez)
            m_file.write(UTF8_BOM);
        return rez;
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

        std::ostringstream ostr;
        const auto curDateTime = QDateTime::currentDateTime();
        ostr << curDateTime.date().toString(Qt::ISODate).toUtf8().constData()<<" "<<curDateTime.time().toString(Qt::ISODate).toUtf8().constData()
            << " " << std::setw(6) << QByteArray::number((qint64)QThread::currentThread()->currentThreadId(), 16).constData()
#ifdef Q_OS_LINUX
            << " ("<<std::setw(6)<<gettid()<<")"
#endif
            << " " << std::setw(7) << qn_logLevelNames[logLevel] << ": " << msg.toUtf8().constData() << "\r\n";
        ostr.flush();

        const std::string& str = ostr.str();
        {
            QMutexLocker mutx(&m_mutex);
            if (!m_file.isOpen()) {

                switch (logLevel) {
                case cl_logERROR:
                case cl_logWARNING:
                {
                    QTextStream out(stderr);
                    out << str.c_str();
                    break;
                }
                default:
                {
                    QTextStream out(stdout);
                    out << str.c_str();
                }
                }
                return;
            }
            m_file.write(str.c_str());
            m_file.flush();
            if (m_file.size() >= m_maxFileSize)
                openNextFile();
        }
    }

    QString syncCurrFileName() const
    {
        QMutexLocker lock(&m_mutex);
        return m_baseName + QLatin1String(".log");
    }

private:
    void openNextFile()
    {
        m_file.close(); // close current file

        int max_existing_num = m_maxBackupFiles;
        while (max_existing_num)
        {
            if (QFile::exists(backupFileName(max_existing_num)))
                break;
            --max_existing_num;
        }

        if (max_existing_num<m_maxBackupFiles) // if we do not need to delete backupfiles
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

        m_file.open(QIODevice::WriteOnly | QIODevice::Append);
        if (m_file.size() == 0)
            m_file.write(UTF8_BOM);
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

    QFile m_file;

    mutable QMutex m_mutex;
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
        QMutexLocker lk( &m_mutex );
        QnLog*& log = m_logs[logID];
        if( !log )
            log = new QnLog();
        return log;
    }

    bool put( int logID, QnLog* log )
    {
        QMutexLocker lk( &m_mutex );
        return m_logs.insert( std::make_pair( logID, log ) ).second;
    }

private:
    std::map<int, QnLog*> m_logs;
    QMutex m_mutex;
};

Q_GLOBAL_STATIC(QnLogs, qn_logsInstance);


QnLog::QnLog()
:
    d( new QnLogPrivate() )
{
}

QnLog::~QnLog()
{
    delete d;
}

QnLog* QnLog::instance( int logID )
{
    return qn_logsInstance()->get(logID);
}

bool QnLog::create(const QString& baseName, quint32 maxFileSize, quint8 maxBackupFiles, QnLogLevel logLevel) {
    if(!d)
        return false;

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

QString QnLog::logFileName( int logID )
{
    return instance(logID)->d->syncCurrFileName();
}

void QnLog::initLog(const QString& logLevelStr, int logID) {
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
    QnLog::instance(logID)->setLogLevel(logLevel);

    if (needWarnLogLevel) {
        NX_LOG(QLatin1String("================================================================================="), cl_logALWAYS);
        NX_LOG(lit("Unknown log level specified. Using level %1").arg(QnLog::logLevelToString(logLevel)), cl_logALWAYS);
    }

}

bool QnLog::initLog( QnLog* externalInstance, int logID )
{
    return qn_logsInstance()->put( logID, externalInstance );
}
