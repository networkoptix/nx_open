#ifndef QN_LOG_H
#define QN_LOG_H

#include <QtCore/QMutex>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDebug>

// TODO: #Elric #enum
enum QnLogLevel { cl_logUNKNOWN, cl_logALWAYS, cl_logERROR, cl_logWARNING, cl_logINFO, cl_logDEBUG1, cl_logDEBUG2 };

class QnLogPrivate;

class QN_EXPORT QnLog {
public:
    bool create(const QString &baseName, quint32 maxFileSize, quint8 maxBackupFiles, QnLogLevel logLevel);

    void setLogLevel(QnLogLevel logLevel);
    QnLogLevel logLevel() const;

    void log(const QString &msg, QnLogLevel logLevel);
    void log(QnLogLevel logLevel, const char *format, ...);

#define QN_LOG_BODY(ARGS)                                                       \
        if(!isActive(logLevel))                                                 \
            return;                                                             \
                                                                                \
        QString message;                                                        \
        QTextStream stream(&message);                                           \
        stream << ARGS;                                                         \
        log(message, logLevel);                                                 \

    template<class T0>
    void log(const T0 &arg0, QnLogLevel logLevel) {
        QN_LOG_BODY(arg0);
    }

    template<class T0, class T1>
    void log(const T0 &arg0, const T1 &arg1, QnLogLevel logLevel) {
        QN_LOG_BODY(arg0 << arg1);
    }

    template<class T0, class T1, class T2>
    void log(const T0 &arg0, const T1 &arg1, const T2 &arg2, QnLogLevel logLevel) {
        QN_LOG_BODY(arg0 << arg1 << arg2);
    }

    template<class T0, class T1, class T2, class T3>
    void log(const T0 &arg0, const T1 &arg1, const T2 &arg2, const T3 &arg3, QnLogLevel logLevel) {
        QN_LOG_BODY(arg0 << arg1 << arg2 << arg3);
    }
#undef QN_LOG_BODY
    
    static void initLog(const QString &logLevelStr);
    //!Initializes log with external instance \a externalInstance
    /*!
        Introduced to allow logging in dynamically loaded plugins
    */
    static void initLog(QnLog *externalInstance);
    static QString logFileName();
    static QnLog *instance();
    
    static QnLogLevel logLevelFromString(const QString &value);
    static QString logLevelToString(QnLogLevel value);

protected:
    QnLog(QnLogPrivate *d);

private:
    bool isActive(QnLogLevel loglevel) const {
        return d && loglevel <= logLevel();
    }

private:
    QnLogPrivate *d;
};

#define CL_LOG(level)                                                           \
    if (level > cl_log.logLevel()) {} else                                      \

#define NX_LOG(msg, level)                \
    {                                     \
        if( level <= cl_log.logLevel() )  \
            cl_log.log( msg, level );     \
    }

#define cl_log (*QnLog::instance())

QN_EXPORT void qnLogMsgHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg);


template<class T>
QString toDebugString(const T &value) {
    QString result;
    QDebug stream(&result);
    stream << value;
    return result;
}

#endif // QN_LOG_H
