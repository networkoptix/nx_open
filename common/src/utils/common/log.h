#ifndef log_h_109
#define log_h_109

#include <QMutex>
#include <QFile>

#include "base.h"

enum CLLogLevel {cl_logALWAYS, cl_logERROR, cl_logWARNING, cl_logINFO, cl_logDEBUG1, cl_logDEBUG2 };

struct CLLogListner
{
    virtual void onLogMsg(const QString& msg)  = 0;
};

class QN_EXPORT CLLog
{
public:
    CLLog();
    bool create(const QString& base_name,
                quint32 max_file_size,
                quint8 maxBackupFiles,
                CLLogLevel loglevel = cl_logINFO);

    void setLogLevel(CLLogLevel loglevel);
    inline CLLogLevel getLoglevel()
    {
        return m_loglevel;
    }

    void setLoglistner(CLLogListner* loglistner);

    void log(const QString& msg, CLLogLevel loglevel);
    void log(const QString& msg, int val, CLLogLevel loglevel);
    void log(const QString& msg, qint64 val, CLLogLevel loglevel);
    void log(const QString& msg, qreal val, CLLogLevel loglevel);
    void log(const QString& msg1, const QString& msg2, CLLogLevel loglevel);
    void log(const QString& msg1, int val, const QString& msg2, CLLogLevel loglevel);
    void log(const QString& msg1, int val, const QString& msg2, int val2, CLLogLevel loglevel);
    void log(CLLogLevel loglevel, const char* format, ...);

private:

    void openNextFile();
    QString backupFileName(quint8 num) const;
    QString currFileName() const;

    quint32 m_max_file_size;
    quint8 m_maxBackupFiles;
    quint8 m_cur_num;
    QString m_base_name;
    CLLogLevel m_loglevel;

    QFile m_file;

    CLLogListner *m_loglistner;

    QMutex m_mutex;
};

QN_EXPORT extern CLLog cl_log;

#define CL_LOG(level)\
    if (level > cl_log.getLoglevel());\
    else

QN_EXPORT void clLogMsgHandler(QtMsgType type, const char *msg);

#endif //log_h_109
