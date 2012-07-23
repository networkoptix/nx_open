#include "log.h"
#include <QTextStream>
#include <QThread>
#include <QDateTime>

char *cl_log_msg[] = {"UNKNOWN", "ALWAYS", "ERROR", "WARNING", "INFO", "DEBUG", "DEBUG2" };

QnLogLevel QnLog::logLevelFromString(const QString &value) {
    QString str = value.toUpper().trimmed();
    for (int i = 0; i < sizeof(cl_log_msg)/sizeof(char*); ++i) {
        if (str == QLatin1String(cl_log_msg[i]))
            return QnLogLevel(i);
    }

    return cl_logUNKNOWN;
}

QString QnLog::logLevelToString(QnLogLevel value) {
    return QLatin1String(cl_log_msg[value]);
}

// -------------------------------------------------------------------------- //
// QnLogPrivate
// -------------------------------------------------------------------------- //
class QnLogPrivate {
public:
    QnLogPrivate():
        m_logLevel(static_cast<QnLogLevel>(0)) /* Log nothing by default. */
    {}

    bool create(const QString& baseName, quint32 maxFileSize, quint8 maxBackupFiles, QnLogLevel logLevel) 
    {
        m_baseName = baseName;
        m_maxFileSize = maxFileSize;
        m_maxBackupFiles = maxBackupFiles;
        m_curNum = 0;

        m_logLevel = logLevel;

        m_file.setFileName(currFileName());

        return m_file.open(QIODevice::WriteOnly | QIODevice::Append);
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

        QMutexLocker mutx(&m_mutex);

        if (!m_file.isOpen())
            return;

        QString th;
        QTextStream textStream(&th);
        hex(textStream) << (quint64)QThread::currentThread()->currentThreadId();

        QTextStream fstr(&m_file);
        fstr << QDateTime::currentDateTime().toString(QLatin1String("ddd MMM d yy  hh:mm:ss.zzz"))
            << QLatin1String(" Thread ") << th
            << QLatin1String(" (") << QString::fromAscii(cl_log_msg[logLevel]) << QLatin1String("): ") << msg << QLatin1String("\r\n");
        fstr.flush();

        if (m_file.size() >= m_maxFileSize)
            openNextFile();
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

    QMutex m_mutex;
};

Q_GLOBAL_STATIC(QnLogPrivate, qn_logPrivateInstance);


// -------------------------------------------------------------------------- //
// QnLog
// -------------------------------------------------------------------------- //
QnLog::QnLog(QnLogPrivate *d): d(d) {}

QnLog QnLog::instance() {
    return QnLog(qn_logPrivateInstance());
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

    d->log(QString::fromLocal8Bit(buffer), logLevel);
    va_end(args);
}

void qnLogMsgHandler(QtMsgType type, const char *msg) {
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

    cl_log.log(QString::fromLocal8Bit(msg), logLevel);
}


void QnLog::initLog(const QString& logLevelStr) {
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
    cl_log.setLogLevel(logLevel);

    CL_LOG(cl_logALWAYS) {
        cl_log.log(QLatin1String("================================================================================="), cl_logALWAYS);
        if (needWarnLogLevel) 
            cl_log.log("Unknown log level specified. Using level ", QnLog::logLevelToString(logLevel), cl_logALWAYS);
    }
}
