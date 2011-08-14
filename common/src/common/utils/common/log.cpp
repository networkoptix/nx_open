#include "log.h"

#include <qplatformdefs.h>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QMutex>
#include <QtCore/QTextStream>
#include <QtCore/QThread>

CLLog cl_log;

static const char *cl_log_msg[] = { "ALWAYS", "ERROR", "WARNING", "INFO", "DEBUG1", "DEBUG2" };

class CLLog::Private
{
public:
    quint32 max_file_size;
    quint8 maxBackupFiles;
    quint8 cur_num;
    QString base_name;
    CLLogLevel loglevel;

    QFile file;

    CLLogListner *loglistner;

    QMutex mutex;
};

CLLog::CLLog()
    : d(new Private)
{
    d->loglistner = 0;
}

CLLog::~CLLog()
{
    delete d;
}

bool CLLog::create(const QString& base_name,
                   quint32 max_file_size,
                    quint8 maxBackupFiles,
                    CLLogLevel loglevel )
{
    d->base_name = base_name;
    d->max_file_size = max_file_size;
    d->maxBackupFiles = maxBackupFiles;
    d->cur_num = 0;

    d->loglevel = loglevel;

    d->file.setFileName(currFileName());

    return d->file.open(QIODevice::WriteOnly | QIODevice::Append);

}

void CLLog::setLogLevel(CLLogLevel loglevel)
{
    d->loglevel = loglevel;
}

CLLogLevel CLLog::getLoglevel()
{
    return d->loglevel;
}

void CLLog::setLoglistner(CLLogListner * loglistner)
{
    d->loglistner = loglistner;
}

void CLLog::log(const QString& msg, int val, CLLogLevel loglevel)
{
    if (loglevel > d->loglevel)
        return;

    QString str;
    QTextStream(&str) << msg << val;

    log(str, loglevel);
}

void CLLog::log(const QString& msg, qint64 val, CLLogLevel loglevel)
{
    if (loglevel > d->loglevel)
        return;

    QString str;
    QTextStream(&str) << msg << val;

    log(str, loglevel);
}


void CLLog::log(const QString& msg1, const QString& msg2, CLLogLevel loglevel)
{
    if (loglevel > d->loglevel)
        return;

    QString str;
    QTextStream(&str) << msg1 << msg2;

    log(str, loglevel);
}

void CLLog::log(const QString& msg1, int val, const QString& msg2, CLLogLevel loglevel)
{
    if (loglevel > d->loglevel)
        return;

    QString str;
    QTextStream(&str) << msg1 << val << msg2;

    log(str, loglevel);

}

void CLLog::log(const QString& msg1, int val, const QString& msg2, int val2, CLLogLevel loglevel)
{
    if (loglevel > d->loglevel)
        return;

    QString str;
    QTextStream(&str) << msg1 << val << msg2 << val2;

    log(str, loglevel);

}

void CLLog::log(const QString& msg, qreal val, CLLogLevel loglevel)
{
    if (loglevel > d->loglevel)
        return;

    QString str;
    QTextStream(&str) << msg << val;

    log(str, loglevel);

}

void CLLog::log(const QString& msg, CLLogLevel loglevel)
{
    if (loglevel > d->loglevel)
        return;

    QMutexLocker locker(&d->mutex);

    if (d->loglistner)
        d->loglistner->onLogMsg(msg);

    if (!d->file.isOpen())
        return;

    QString th;
    QTextStream textStream(&th);
    hex(textStream) << (quint64)QThread::currentThread();

    QTextStream fstr(&d->file);
    fstr << QDateTime::currentDateTime().toString(QLatin1String("ddd MMM d yy  hh:mm:ss.zzz"))
         << QLatin1String(" Thread ") << th
         << QLatin1String(" (") << QString::fromAscii(cl_log_msg[loglevel]) << QLatin1String("): ") << msg << QLatin1String("\r\n");
    fstr.flush();

    if (d->file.size() >= d->max_file_size)
        openNextFile();

}

void CLLog::log(CLLogLevel loglevel, const char* format, ...)
{
    static const int MAX_MESSAGE_SIZE = 1024;

    char buffer[MAX_MESSAGE_SIZE];
    va_list args;

    va_start(args, format);

    QT_VSNPRINTF(buffer, MAX_MESSAGE_SIZE, format, args);
    cl_log.log(QString::fromLocal8Bit(buffer), loglevel);

    va_end(args);
}

void CLLog::openNextFile()
{
    d->file.close(); // close current file

    int max_existing_num = d->maxBackupFiles;
    while (max_existing_num)
    {
        if (QFile::exists(backupFileName(max_existing_num)))
            break;
        --max_existing_num;
    }

    if (max_existing_num<d->maxBackupFiles) // if we do not need to delete backupfiles
    {
        QFile::rename(currFileName(), backupFileName(max_existing_num+1));
    }
    else // we need to delete one backup file
    {
        QFile::remove(backupFileName(1)); // delete the oldest file

        for (int i = 2; i <= d->maxBackupFiles; ++i) // shift all file names by one
            QFile::rename(backupFileName(i), backupFileName(i-1));

        QFile::rename(currFileName(), backupFileName(d->maxBackupFiles)); // move current to the most latest backup
    }

    d->file.open(QIODevice::WriteOnly | QIODevice::Append);

}

QString CLLog::currFileName() const
{
    return d->base_name + QLatin1String(".log");
}

QString CLLog::backupFileName(quint8 num) const
{
    QString result;
    QTextStream stream(&result);

    char cnum[4];

    QT_SNPRINTF(cnum, 4, "%.3d", num);

    stream << d->base_name << cnum << ".log";
    return result;
}
