#ifndef CRASH_REPORTER_H
#define CRASH_REPORTER_H

#include "ec2_statictics_reporter.h"

#include <QDir>
#include <set>

namespace ec2 {

class CrashReporter
    : public QObject
{
    Q_OBJECT

public:
    /** Scans for local reports and sends them to the statistics server asincronously
     *  \param admin is used to configure the process (bt system settings)
     *
     *  \note Might be used on the start up in  every binary which generates crash dumps by
     *        \class win32_exception or \class linux_exception
     */
    static void scanForProblemsAndReport(QnUserResourcePtr admin);

    /** Sends \param crash to \param serverApi asincronously
     *  \note Might be used for debug purposes
     */
    static void send(const QUrl& serverApi, const QFileInfo& crash, bool auth);

private slots:
    void finishReport(nx_http::AsyncHttpClientPtr httpClient) const;

private:
    CrashReporter(const QFileInfo& crashFile, QObject* parent);
    nx_http::HttpHeaders makeHttpHeaders() const;

    const QFileInfo m_crashFile;
    static std::set<nx_http::AsyncHttpClientPtr> c_activeHttpClients;
};

} // namespace ec2

#endif // CRASH_REPORTER_H
