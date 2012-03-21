#ifndef QN_LOG_H
#define QN_LOG_H

#include <QMutex>
#include <QFile>

#include "base.h"

enum QnLogLevel {cl_logUNKNOWN, cl_logALWAYS, cl_logERROR, cl_logWARNING, cl_logINFO, cl_logDEBUG1, cl_logDEBUG2 };

class QnLogPrivate;

class QN_EXPORT QnLog
{
public:
    bool create(const QString& baseName, quint32 maxFileSize, quint8 maxBackupFiles, QnLogLevel logLevel);

    void setLogLevel(QnLogLevel loglevel);
    QnLogLevel logLevel() const;

    void log(const QString& msg, QnLogLevel loglevel);
    void log(const QString& msg, int val, QnLogLevel loglevel);
    void log(const QString& msg, qint64 val, QnLogLevel loglevel);
    void log(const QString& msg, qreal val, QnLogLevel loglevel);
    void log(const QString& msg1, const QString& msg2, QnLogLevel loglevel);
    void log(const QString& msg1, int val, const QString& msg2, QnLogLevel loglevel);
    void log(const QString& msg1, int val, const QString& msg2, int val2, QnLogLevel loglevel);
    void log(QnLogLevel loglevel, const char* format, ...);

    static void initLog(const QString& logLevelStr);
    static QnLog instance();
    static QnLogLevel logLevelFromString(const QString& value);
    static QString logLevelToString(QnLogLevel value);

protected:
    QnLog(QnLogPrivate *d);

private:
    QnLogPrivate *d;
};

#define CL_LOG(level)                                                           \
    if (level > cl_log.logLevel()) {}                                           \
    else

#define cl_log (QnLog::instance())

QN_EXPORT void qnLogMsgHandler(QtMsgType type, const char *msg);

#endif // QN_LOG_H
