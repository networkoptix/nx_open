#include "log.h"
#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>

#include <QtCore/QThread>
#include <QtCore/QDateTime>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
	#include <sys/types.h>
	#include <linux/unistd.h>
	static pid_t gettid(void) { return syscall(__NR_gettid); }
#endif


const char *qn_logLevelNames[] = {"UNKNOWN", "NONE", "ALWAYS", "ERROR", "WARNING", "INFO", "DEBUG", "DEBUG2"};

QnLogLevel QnLog::logLevelFromString(const QString &value) {
    const QString str = value.toUpper().trimmed();

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

        std::ostringstream ostr;
        const auto curDateTime = QDateTime::currentDateTime();
        const auto curTime = curDateTime.time();
        ostr << curDateTime.date().toString(Qt::ISODate).toUtf8().constData()<<" "
            <<curTime.toString(Qt::ISODate).toUtf8().constData()<<"."<<std::setw(3)<<std::setfill('0')<<curTime.msec()<<std::setfill(' ')
            << " " << std::setw(6) <<
#ifdef Q_OS_LINUX
            gettid()
#else
            QByteArray::number((qint64)QThread::currentThread()->currentThreadId(), 16).constData()
#endif
            << " " << std::setw(7) << qn_logLevelNames[logLevel] << ": " << msg.toUtf8().constData() << "\r\n";

        {
            QnMutexLocker mutx( &m_mutex );
            if (m_file.fail())
            {
                switch (logLevel)
                {
                    case cl_logERROR:
                    case cl_logWARNING:
                        std::cerr << ostr.str();
                        std::cerr.flush();
                        break;

                    default:
                        std::cout << ostr.str();
                        std::cout.flush();
                        break;
                }
            }
            else
            {
                m_file << ostr.str();
                m_file.flush();
                if (m_file.tellp() >= m_maxFileSize)
                    openNextFile();
            }
        }
    }

    QString syncCurrFileName() const
    {
        QnMutexLocker lock( &m_mutex );
        return m_baseName + QLatin1String(".log");
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
        #ifdef Q_OS_WIN
            m_file.open(currFileName().toStdWString(), std::ios_base::app | std::ios_base::out);
        #else
            m_file.open(currFileName().toStdString(), std::ios_base::app | std::ios_base::out);
        #endif

        if (m_file.fail())
            return;

        // Ensure 1st char is UTF8 BOM
        if (m_file.tellp() == std::fstream::pos_type(0))
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
    mutable QnMutex m_mutex;
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
        QnMutexLocker lk( &m_mutex );
        QnLog*& log = m_logs[logID];
        if( !log )
            log = new QnLog();
        return log;
    }

    bool exists( int logID )
    {
        QnMutexLocker lk( &m_mutex );
        return m_logs.find(logID) != m_logs.cend();
    }

    bool put( int logID, QnLog* log )
    {
        QnMutexLocker lk( &m_mutex );
        return m_logs.insert( std::make_pair( logID, log ) ).second;
    }

private:
    std::map<int, QnLog*> m_logs;
    QnMutex m_mutex;
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

bool QnLog::instanceExists( int logID ) {
    return qn_logsInstance()->exists(logID);
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
