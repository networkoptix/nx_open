#ifndef CRASH_REPORTER_H
#define CRASH_REPORTER_H

#include "ec2_statictics_reporter.h"

#include <utils/common/concurrent.h>
#include <nx/utils/thread/mutex.h>

#include <QDir>
#include <QSettings>
#include <set>
#include <common/common_module_aware.h>

namespace ec2 {

class CrashReporter: public QnCommonModuleAware
{
public:
    CrashReporter(QnCommonModule* commonModule);
    ~CrashReporter();

    /** Scans for local reports and sends them to the statistics server asynchronously
     *  \param admin is used to configure the process (bt system settings)
     *
     *  \note Might be used on the start up in  every binary which generates crash dumps by
     *        \class win32_exception or \class linux_exception
     */
    bool scanAndReport(QSettings* settings);
    void scanAndReportAsync(QSettings* settings);

    /** Executes /fn scanAndReportAsync by timer. Useful to collect reports coming late.
     */
    void scanAndReportByTimer(QSettings* settings);

    /** Sends \param crash to \param serverApi asynchronously
     *  \note Might be used for debug purposes
     */
    bool send(const QUrl& serverApi, const QFileInfo& crash, QSettings* settings);

private:
    friend class ReportData;

    QnMutex m_mutex;
    QnConcurrent::QnFuture<bool> m_activeCollection;
    nx_http::AsyncHttpClientPtr m_activeHttpClient;
    bool m_terminated;
    boost::optional<qint64> m_timerId;
};

class ReportData: public QObject
{
    Q_OBJECT

public:
    ReportData(const QFileInfo& crashFile, QSettings* settings,
               CrashReporter& host, QObject* parent = 0);

    nx_http::HttpHeaders makeHttpHeaders() const;

public slots:
    void finishReport(nx_http::AsyncHttpClientPtr httpClient);

private:
    const QFileInfo m_crashFile;
    QSettings* m_settings;
    CrashReporter& m_host;
};

} // namespace ec2

#endif // CRASH_REPORTER_H
