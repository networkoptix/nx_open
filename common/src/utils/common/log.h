#ifndef QN_LOG_H
#define QN_LOG_H

#include <utils/thread/mutex.h>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDebug>
#include <mutex>

// TODO: #Elric #enum
enum QnLogLevel {
    cl_logUNKNOWN,
    cl_logALWAYS,
    cl_logERROR,
    cl_logWARNING,
    cl_logINFO,
    cl_logDEBUG1,
    cl_logDEBUG2
};

class QnLogPrivate;

class QN_EXPORT QnLog {
public:
    static const int MAIN_LOG_ID = 0;
    static const int CUSTOM_LOG_BASE_ID = 1;
    static const int HTTP_LOG_INDEX = CUSTOM_LOG_BASE_ID + 1;
    static const int EC2_TRAN_LOG = CUSTOM_LOG_BASE_ID + 2;
    static const int HWID_LOG = CUSTOM_LOG_BASE_ID + 3;

    QnLog();
    ~QnLog();

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
    
    class Logs
    {
    public:
        const std::unique_ptr< QnLog >& get( int logID = MAIN_LOG_ID );
        bool init( int logID, std::unique_ptr< QnLog > log );
        bool init( int logID, const QString& logLevelStr );

    private:
        QnMutex m_mutex;
        std::map< int, std::unique_ptr< QnLog > > m_logs;
    };

    static void initLog(const QString &logLevelStr, int logID = MAIN_LOG_ID);
    static bool initLog(QnLog *externalInstance, int logID = MAIN_LOG_ID);

    static QString logFileName( int logID = MAIN_LOG_ID );
    static const std::shared_ptr< Logs >& logs();
    static const std::unique_ptr< QnLog >& instance( int logID = MAIN_LOG_ID );
    
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

    static std::once_flag s_onceFlag;
    static std::shared_ptr< Logs > s_instance;
};

#define CL_LOG(level)                                           \
    if (level > cl_log.logLevel()) {} else                      \


#define NX_LOG_MSVC_EXPAND(x) x
#define GET_NX_LOG_MACRO(_1, _2, _3, NAME, ...) NAME
#define NX_LOG(...) NX_LOG_MSVC_EXPAND(GET_NX_LOG_MACRO(__VA_ARGS__, NX_LOG_3, NX_LOG_2)(__VA_ARGS__))

#define NX_LOG_2(msg, level)                    \
    if( auto _nx_logs = QnLog::logs() )         \
    {                                           \
        auto& _nx_log = _nx_logs->get();        \
        if( level <= _nx_log->logLevel() )      \
            _nx_log->log( msg, level );         \
    }

#define NX_LOG_3(logID, msg, level)             \
    if( auto _nx_logs = QnLog::logs() )         \
    {                                           \
        auto& _nx_log = _nx_logs->get( logID ); \
        if( level <= _nx_log->logLevel() )      \
            _nx_log->log( msg, level );         \
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
