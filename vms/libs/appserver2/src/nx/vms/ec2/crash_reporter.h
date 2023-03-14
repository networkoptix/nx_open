// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/utils/concurrent.h>
#include <nx/utils/property_storage/storage.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>
#include <nx/vms/common/system_context_aware.h>

namespace nx::utils { class TimerManager; }

namespace ec2 {

class CrashReporter: public nx::vms::common::SystemContextAware
{
public:
    class Settings: public nx::utils::property_storage::Storage
    {
    public:
        Settings(const QString& settingsDir);
        Property<QString> lastCrashDate{this, "lastCrashDate"};
    };

    CrashReporter(
        nx::vms::common::SystemContext* systemContext,
        nx::utils::TimerManager* timerManager,
        const QString& settingsDir);
    ~CrashReporter();

    /** Scans for local reports and sends them to the statistics server asynchronously
     *  \param admin is used to configure the process (bt system settings)
     *
     *  \note Might be used on the start up in  every binary which generates crash dumps by
     *        \class win32_exception or \class linux_exception
     */
    bool scanAndReport();
    void scanAndReportAsync();

    /** Executes /fn scanAndReportAsync by timer. Useful to collect reports coming late.
     */
    void scanAndReportByTimer();

    /** Sends \param crash to \param serverApi asynchronously
     *  \note Might be used for debug purposes
     */
    bool send(const nx::utils::Url& serverApi, const QFileInfo& crash);

    Settings* settings() const;

private:
    friend class ReportData;

    nx::utils::TimerManager* const m_timerManager;
    nx::Mutex m_mutex;
    nx::utils::concurrent::Future<bool> m_activeCollection;
    nx::network::http::AsyncHttpClientPtr m_activeHttpClient;
    bool m_terminated;
    std::optional<qint64> m_timerId;
    std::unique_ptr<Settings> m_settings;
};

class ReportData: public QObject
{
    Q_OBJECT

public:
    ReportData(const QFileInfo& crashFile, CrashReporter& host, QObject* parent = 0);

    nx::network::http::HttpHeaders makeHttpHeaders() const;

public slots:
    void finishReport(nx::network::http::AsyncHttpClientPtr httpClient);

private:
    const QFileInfo m_crashFile;
    CrashReporter& m_host;
};

} // namespace ec2
