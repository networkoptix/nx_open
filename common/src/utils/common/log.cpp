#include "log.h"
#include <QTextStream>
#include <QThread>
#include <QDateTime>

Q_GLOBAL_STATIC(CLLog, qn_logInstance);

char *cl_log_msg[] = { "ALWAYS", "ERROR", "WARNING", "INFO", "DEBUG1", "DEBUG2" };

CLLog::CLLog():
    m_logListener(0)
{
}

CLLog *CLLog::instance() {
    return qn_logInstance();
}

bool CLLog::create(const QString& base_name,
                   quint32 max_file_size,
                    quint8 maxBackupFiles,
                    CLLogLevel loglevel )
{
    m_base_name = base_name;
    m_max_file_size = max_file_size;
    m_maxBackupFiles = maxBackupFiles;
    m_cur_num = 0;

    m_loglevel = loglevel;

    m_file.setFileName(currFileName());

    return m_file.open(QIODevice::WriteOnly | QIODevice::Append);

}

void CLLog::setLogLevel(CLLogLevel loglevel)
{
    m_loglevel = loglevel;
}

void CLLog::setLogListener(CLLogListener * loglistner)
{
    m_logListener = loglistner;
}

void CLLog::log(const QString& msg, int val, CLLogLevel loglevel)
{
    if (loglevel > m_loglevel)
        return;

    QString str;
    QTextStream(&str) << msg << val;

    log(str, loglevel);
}

void CLLog::log(const QString& msg, qint64 val, CLLogLevel loglevel)
{
    if (loglevel > m_loglevel)
        return;

    QString str;
    QTextStream(&str) << msg << val;

    log(str, loglevel);
}


void CLLog::log(const QString& msg1, const QString& msg2, CLLogLevel loglevel)
{
    if (loglevel > m_loglevel)
        return;

    QString str;
    QTextStream(&str) << msg1 << msg2;

    log(str, loglevel);
}

void CLLog::log(const QString& msg1, int val, const QString& msg2, CLLogLevel loglevel)
{
    if (loglevel > m_loglevel)
        return;

    QString str;
    QTextStream(&str) << msg1 << val << msg2;

    log(str, loglevel);
}

void CLLog::log(const QString& msg1, int val, const QString& msg2, int val2, CLLogLevel loglevel)
{
    if (loglevel > m_loglevel)
        return;

    QString str;
    QTextStream(&str) << msg1 << val << msg2 << val2;

    log(str, loglevel);
}

void CLLog::log(const QString& msg, qreal val, CLLogLevel loglevel)
{
    if (loglevel > m_loglevel)
        return;

    QString str;
    QTextStream(&str) << msg << val;

    log(str, loglevel);
}

void CLLog::log(const QString& msg, CLLogLevel loglevel)
{
//	return;

    if (loglevel > m_loglevel)
        return;

    QMutexLocker mutx(&m_mutex);

    if (m_logListener)
        m_logListener->onLogMsg(msg);

    if (!m_file.isOpen())
        return;

    QString th;
    QTextStream textStream(&th);
    hex(textStream) << (quint64)QThread::currentThread();

    QTextStream fstr(&m_file);
    fstr << QDateTime::currentDateTime().toString(QLatin1String("ddd MMM d yy  hh:mm:ss.zzz"))
         << QLatin1String(" Thread ") << th
         << QLatin1String(" (") << QString::fromAscii(cl_log_msg[loglevel]) << QLatin1String("): ") << msg << QLatin1String("\r\n");
    fstr.flush();

    if (m_file.size() >= m_max_file_size)
        openNextFile();
}

void CLLog::log(CLLogLevel loglevel, const char* format, ...)
{
    static const int MAX_MESSAGE_SIZE = 1024;

    char buffer[MAX_MESSAGE_SIZE];
    va_list args;

    va_start(args, format);
#ifdef Q_CC_MSVC
    vsnprintf_s(buffer, MAX_MESSAGE_SIZE, MAX_MESSAGE_SIZE, format, args);
#else
    vsnprintf(buffer, MAX_MESSAGE_SIZE, format, args);
#endif

    log(QString::fromLocal8Bit(buffer), loglevel);
    va_end(args);
}

void CLLog::openNextFile()
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

QString CLLog::currFileName() const
{
    return m_base_name + QLatin1String(".log");
}

QString CLLog::backupFileName(quint8 num) const
{
    QString result;
    QTextStream stream(&result);

    char cnum[4];

#ifdef Q_CC_MSVC
    sprintf_s(cnum, 4, "%.3d", num);
#else
    snprintf(cnum, 4, "%.3d", num);
#endif

    stream << m_base_name << cnum << ".log";
    return result;
}

void clLogMsgHandler(QtMsgType type, const char *msg)
{
    CLLogLevel logLevel;
    switch (type) {
    case QtFatalMsg:
    case QtCriticalMsg:
        logLevel = cl_logERROR;
        break;
    case QtWarningMsg:
        logLevel = cl_logWARNING;
        break;
    case QtDebugMsg:
        // fall through
    default:
        logLevel = cl_logINFO;
        break;
    }
    cl_log.log(QString::fromLocal8Bit(msg), logLevel);
}
