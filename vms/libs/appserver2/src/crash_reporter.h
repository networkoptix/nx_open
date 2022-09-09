// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <set>

#include <QtCore/QDir>
#include <QtCore/QSettings>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/utils/concurrent.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>
#include <nx/vms/common/system_context_aware.h>

namespace ec2 {

class CrashReporter: public nx::vms::common::SystemContextAware
{
public:
    CrashReporter(nx::vms::common::SystemContext* systemContext);
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
    bool send(const nx::utils::Url& serverApi, const QFileInfo& crash, QSettings* settings);

private:
    friend class ReportData;

    nx::Mutex m_mutex;
    nx::utils::concurrent::Future<bool> m_activeCollection;
    nx::network::http::AsyncHttpClientPtr m_activeHttpClient;
    bool m_terminated;
    std::optional<qint64> m_timerId;
};

class ReportData: public QObject
{
    Q_OBJECT

public:
    ReportData(const QFileInfo& crashFile, QSettings* settings,
               CrashReporter& host, QObject* parent = 0);

    nx::network::http::HttpHeaders makeHttpHeaders() const;

public slots:
    void finishReport(nx::network::http::AsyncHttpClientPtr httpClient);

private:
    const QFileInfo m_crashFile;
    QSettings* m_settings;
    CrashReporter& m_host;
};

} // namespace ec2
