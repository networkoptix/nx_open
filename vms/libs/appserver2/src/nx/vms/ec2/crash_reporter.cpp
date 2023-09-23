// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "crash_reporter.h"

#include <QtCore/QDateTime>

#include <nx/build_info.h>
#include <nx/utils/app_info.h>
#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/utils/log/log.h>
#include <nx/utils/property_storage/filesystem_backend.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/api/data/os_information.h>
#include <nx/vms/common/application_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/statistics/settings.h>
#include <utils/common/scoped_thread_rollback.h>
#include <utils/common/synctime.h>

#include "ec2_thread_pool.h"

static const QString DATE_FORMAT("yyyy-MM-dd_hh-mm-ss");
static const QString SERVER_API_COMMAND("crashserver/api/report");
static const QString LAST_CRASH("statisticsReportLastCrash");
static const QString SENT_PREFIX("sent_");

static const uint SENDING_MIN_INTERVAL = 24 * 60 * 60; /* secs => a day */
static const uint SENDING_MIN_SIZE = 1 * 1024; /* less then 1kb is not informative */
static const uint SENDING_MAX_SIZE = 32 * 1024 * 1024; /* over 30mb is too big */
static const uint SCAN_TIMER_CYCLE = 10 * 60 * 1000; /* every 10 minutes */
static const uint KEEP_LAST_CRASHES = 10;

static QFileInfoList readCrashes([[maybe_unused]] const QString& prefix = QString())
{
    #if defined( _WIN32 )
        const QDir crashDir(QString::fromStdString(win32_exception::getCrashDirectory()));
        const auto crashFilter = prefix + QString::fromStdString(win32_exception::getCrashPattern());
    #elif defined (Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        const QDir crashDir(QString::fromStdString(linux_exception::getCrashDirectory()));
        const auto crashFilter = prefix + QString::fromStdString(linux_exception::getCrashPattern());
    #else
        const QDir crashDir;
        const QString crashFilter;
        return QFileInfoList(); // do nothing. not implemented
    #endif

    NX_DEBUG(typeid(ec2::CrashReporter), "readCrashes: scan %1 for files %2",
           crashDir.absolutePath(), crashFilter);

    auto files = crashDir.entryInfoList(QStringList() << crashFilter, QDir::Files);
    // Qt has a crossplatform bug in build in sort by QDir::Time
    // - linux: time decrements along the list
    // - windows: time increments along the list
    std::sort(
        files.begin(), files.end(),
        [](const QFileInfo& left, const QFileInfo& right)
        {
            return left.lastModified() > right.lastModified();
        });

    return files;
}

namespace ec2 {

CrashReporter::Settings::Settings(const QString& settingsDir):
    Storage(new nx::utils::property_storage::FileSystemBackend(settingsDir))
{
}

CrashReporter::CrashReporter(
    nx::vms::common::SystemContext* systemContext,
    nx::utils::TimerManager* timerManager,
    const QString& settingsDir)
    :
    nx::vms::common::SystemContextAware(systemContext),
    m_timerManager(timerManager),
    m_terminated(false),
    m_settings(new Settings(settingsDir))
{
}

CrashReporter::~CrashReporter()
{
    std::optional<qint64> timerId;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_terminated = true;
        timerId = m_timerId;
    }

    if (timerId)
        m_timerManager->joinAndDeleteTimer(*timerId);

    // wait for the last scanAndReportAsync
    m_activeCollection.cancel();
    m_activeCollection.waitForFinished();

    // cancel async IO
    nx::network::http::AsyncHttpClientPtr httpClient;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        std::swap(httpClient, m_activeHttpClient);
    }
    if (httpClient)
        httpClient->pleaseStopSync();
}

bool CrashReporter::scanAndReport()
{
    if (nx::build_info::publicationType() == nx::build_info::PublicationType::local)
    {
        NX_DEBUG(this, "Crash reports sending is disabled for local builds");
        return false;
    }

    const auto& globalSettings = systemSettings();

    // remove old crashes
    {
        auto allCrashes = readCrashes("*");
        for (uint i = 0; i < KEEP_LAST_CRASHES && !allCrashes.isEmpty(); ++i)
            allCrashes.pop_front();

        for (const auto& crash : allCrashes)
            QFile::remove(crash.absoluteFilePath());
    }

    if (!globalSettings->isInitialized())
        return false;

    if (!globalSettings->isStatisticsAllowed()
        || globalSettings->isNewSystem())
    {
        NX_DEBUG(this, "Automatic report system is disabled");
        return false;
    }

    const auto now = qnSyncTime->currentDateTime().toUTC();
    const auto lastTime = QDateTime::fromString(m_settings->lastCrashDate(), Qt::ISODate);

    if (now < lastTime.addSecs(SENDING_MIN_INTERVAL) &&
        lastTime < now.addSecs(SENDING_MIN_INTERVAL)) // avoid possible long resync problem
    {
        NX_DEBUG(this, "Previous crash was reported %1, exit", lastTime.toString(Qt::ISODate));
        return false;
    }

    const QString configApi = globalSettings->statisticsReportServerApi();
    const QString serverApi = configApi.isEmpty()
        ? nx::vms::statistics::kDefaultStatisticsServer
        : configApi;

    const nx::utils::Url url = QString("%1/%2").arg(serverApi).arg(SERVER_API_COMMAND);

    auto crashes = readCrashes();
    while (!crashes.isEmpty())
    {
        const auto& crash = crashes.first();

        if (crash.size() < SENDING_MIN_SIZE)
        {
            QFile::remove(crash.absoluteFilePath());
            NX_VERBOSE(this, "Remove not informative crash: %1", crash.absolutePath());
        }
        else if (crash.size() < SENDING_MAX_SIZE)
            return CrashReporter::send(url, crash);

        crashes.pop_front();
    }

    return false;
}

void CrashReporter::scanAndReportAsync()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_terminated)
        return;

    // This function is not supposed to be called more then once per binary, but anyway:
    if (m_activeCollection.isInProgress())
    {
        NX_ERROR(this, "Previous report is in progress, exit");
        return;
    }

    NX_DEBUG(this, "Start new async scan for reports");
    m_activeCollection = nx::utils::concurrent::run(Ec2ThreadPool::instance(), [=](){
        // \class nx::utils::concurrent posts a job to \class Ec2ThreadPool rather than create new
        // real thread, we need to reserve a thread to avoid possible deadlock
        QnScopedThreadRollback reservedThread( 1, Ec2ThreadPool::instance() );
        return scanAndReport();
    });
}

void CrashReporter::scanAndReportByTimer()
{
    scanAndReportAsync();

    NX_MUTEX_LOCKER lk(&m_mutex);
    if (!m_terminated)
    {
        m_timerId = m_timerManager->addTimer(
            std::bind(&CrashReporter::scanAndReportByTimer, this),
            std::chrono::milliseconds(SCAN_TIMER_CYCLE));
    }
}

bool CrashReporter::send(const nx::utils::Url& serverApi, const QFileInfo& crash)
{
    auto filePath = crash.absoluteFilePath();
    QFile file(filePath);
    file.open(QFile::ReadOnly);
    auto content = file.readAll();
    if (content.size() == 0)
    {
        NX_WARNING(this, "Error: %1 is not readable or empty: %2", filePath, file.errorString());
        return false;
    }

    auto httpClient = nx::network::http::AsyncHttpClient::create(nx::network::ssl::kDefaultCertificateCheck);
    auto report = new ReportData(crash, *this, httpClient.get());
    QObject::connect(httpClient.get(), &nx::network::http::AsyncHttpClient::done,
                    report, &ReportData::finishReport, Qt::DirectConnection);

    httpClient->setCredentials(nx::network::http::PasswordCredentials(
        nx::vms::statistics::kDefaultUser.toStdString(),
        nx::vms::statistics::kDefaultPassword.toStdString()));
    httpClient->setAdditionalHeaders(report->makeHttpHeaders());

    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_activeHttpClient)
    {
        NX_WARNING(this, "Another report already is in progress!");
        return false;
    }

    NX_INFO(this, "Send %1 to %2", filePath, serverApi.toString());

    httpClient->doPost(serverApi, "application/octet-stream", std::move(content));
    m_activeHttpClient = std::move(httpClient);
    return true;
}

CrashReporter::Settings* CrashReporter::settings() const
{
    return m_settings.get();
}

ReportData::ReportData(const QFileInfo& crashFile, CrashReporter& host, QObject* parent):
    QObject(parent),
    m_crashFile(crashFile),
    m_host(host)
{
}

void ReportData::finishReport(nx::network::http::AsyncHttpClientPtr httpClient)
{
    const auto now = qnSyncTime->currentDateTime().toUTC();

    if (!httpClient->hasRequestSucceeded())
    {
        NX_WARNING(this, "Sending %1 to %2 has failed",
            m_crashFile.absoluteFilePath(), httpClient->url().toString());
    }
    else
    {
        NX_DEBUG(this, "Report %1 has been sent successfully", m_crashFile.absoluteFilePath());

        QFile::rename(
            m_crashFile.absoluteFilePath(),
            m_crashFile.absoluteDir().absoluteFilePath(SENT_PREFIX + m_crashFile.fileName()));
    }

    NX_MUTEX_LOCKER lock(&m_host.m_mutex);
    m_host.settings()->lastCrashDate = now.toString(Qt::ISODate);
    NX_ASSERT(!m_host.m_activeHttpClient || m_host.m_activeHttpClient == httpClient);
    m_host.m_activeHttpClient.reset();
}

nx::network::http::HttpHeaders ReportData::makeHttpHeaders() const
{
    const auto fileName = m_crashFile.fileName();

#if defined( _WIN32 )
    const auto binName = fileName.split(QChar('.')).first(); // remove extension (.exe)
#else
    const auto binName = fileName.split(QChar('_')).first();
#endif

    const auto uuidHash = m_host.peerId().toSimpleString().replace("-", "");
    const auto version = nx::utils::AppInfo::applicationFullVersion();
    const auto systemInfo = nx::vms::api::OsInformation::fromBuildInfo().toString();
    const auto systemRuntime = nx::vms::api::OsInformation::currentSystemRuntime();
    const auto system = QString( "%1 %2" ).arg( systemInfo ).arg( systemRuntime )
            .replace( QChar( ' ' ), QChar( '-' ) );

    const auto timestamp = m_crashFile.birthTime().toUTC().toString("yyyy-MM-dd_hh-mm-ss");
    const auto extension = fileName.split(QChar('.')).last();

    nx::network::http::HttpHeaders headers;
    headers.insert(std::make_pair("Nx-Binary", binName.toUtf8()));
    headers.insert(std::make_pair("Nx-Uuid-Hash", uuidHash.toUtf8()));
    headers.insert(std::make_pair("Nx-Version", version.toUtf8()));
    headers.insert(std::make_pair("Nx-System", system.toUtf8()));
    headers.insert(std::make_pair("Nx-Timestamp", timestamp.toUtf8()));
    headers.insert(std::make_pair("Nx-Extension", extension.toUtf8()));
    NX_VERBOSE(NX_SCOPE_TAG, "Report %1 headers %2", fileName, headers);
    return headers;
}

} // namespace ec2
