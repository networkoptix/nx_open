#ifndef CRASH_REPORTER_H
#define CRASH_REPORTER_H

#include "ec2_statictics_reporter.h"

#include <utils/common/concurrent.h>

#include <QDir>
#include <QMutex>
#include <QSettings>
#include <set>

namespace ec2 {

class CrashReporter
{
public:
    ~CrashReporter();

    /** Scans for local reports and sends them to the statistics server asynchronously
     *  \param admin is used to configure the process (bt system settings)
     *
     *  \note Might be used on the start up in  every binary which generates crash dumps by
     *        \class win32_exception or \class linux_exception
     */
    bool scanAndReport(QSettings* settings);
    void scanAndReportAsync(QSettings* settings);

    /** Sends \param crash to \param serverApi asynchronously
     *  \note Might be used for debug purposes
     */
    bool send(const QUrl& serverApi, const QFileInfo& crash, QSettings* settings);

private:
    friend class ReportData;

    QMutex m_mutex;
    QnConcurrent::QnFuture<bool> m_activeCollection;
    std::set<nx_http::AsyncHttpClientPtr> m_activeHttpClients;
};

class ReportData : public QObject
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
