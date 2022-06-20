// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "crash_reporter.h"

#include <QDateTime>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <ec2_thread_pool.h>
#include <nx/build_info.h>
#include <nx/utils/app_info.h>
#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/utils/log/log.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/api/data/os_information.h>
#include <nx/vms/common/application_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/statistics/settings.h>
#include <utils/common/scoped_thread_rollback.h>
#include <utils/common/synctime.h>

static const QString DATE_FORMAT = lit("yyyy-MM-dd_hh-mm-ss");
static const QString SERVER_API_COMMAND = lit("crashserver/api/report");
static const QString LAST_CRASH = lit("statisticsReportLastCrash");
static const QString SENT_PREFIX = lit("sent_");

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

    NX_DEBUG(typeid(ec2::CrashReporter), lit("readCrashes: scan %1 for files %2")
           .arg(crashDir.absolutePath()).arg(crashFilter));

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

CrashReporter::CrashReporter(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_terminated(false)
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
        nx::vms::common::appContext()->timerManager()->joinAndDeleteTimer(*timerId);

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

bool CrashReporter::scanAndReport(QSettings* settings)
{
    if (nx::build_info::publicationType() == nx::build_info::PublicationType::local)
    {
        NX_DEBUG(this, "Crash reports sending is disabled for local builds");
        return false;
    }

    const auto& globalSettings = this->globalSettings();

    // remove old crashes
    {
        auto allCrashes = readCrashes(lit("*"));
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
        NX_DEBUG(this, lit("Automatic report system is disabled"));
        return false;
    }

    const auto now = qnSyncTime->currentDateTime().toUTC();
    const auto lastTime = QDateTime::fromString(
            settings->value(LAST_CRASH, "").toString(), Qt::ISODate);

    if (now < lastTime.addSecs(SENDING_MIN_INTERVAL) &&
        lastTime < now.addSecs(SENDING_MIN_INTERVAL)) // avoid possible long resync problem
    {
        NX_DEBUG(this, lit("Previous crash was reported %1, exit")
                .arg(lastTime.toString(Qt::ISODate)));
        return false;
    }

    const QString configApi = globalSettings->statisticsReportServerApi();
    const QString serverApi = configApi.isEmpty()
        ? nx::vms::statistics::kDefaultStatisticsServer
        : configApi;

    const nx::utils::Url url = lit("%1/%2").arg(serverApi).arg(SERVER_API_COMMAND);

    auto crashes = readCrashes();
    while (!crashes.isEmpty())
    {
        const auto& crash = crashes.first();

        if (crash.size() < SENDING_MIN_SIZE)
        {
            QFile::remove(crash.absoluteFilePath());
            NX_VERBOSE(this, lit("Remove not informative crash: %1")
                .arg(crash.absolutePath()));
        }
        else
        if (crash.size() < SENDING_MAX_SIZE)
            return CrashReporter::send(url, crash, settings);

        crashes.pop_front();
    }

    return false;
}

void CrashReporter::scanAndReportAsync(QSettings* settings)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    // This function is not supposed to be called more then once per binary, but anyway:
    if (m_activeCollection.isInProgress())
    {
        NX_ERROR(this, lit("Previous report is in progress, exit"));
        return;
    }

    NX_DEBUG(this, lit("Start new async scan for reports"));
    m_activeCollection = nx::utils::concurrent::run(Ec2ThreadPool::instance(), [=](){
        // \class nx::utils::concurrent posts a job to \class Ec2ThreadPool rather than create new
        // real thread, we need to reverve a thread to avoid possible deadlock
        QnScopedThreadRollback reservedThread( 1, Ec2ThreadPool::instance() );
        return scanAndReport(settings);
    });
}

void CrashReporter::scanAndReportByTimer(QSettings* settings)
{
    scanAndReportAsync(settings);

    NX_MUTEX_LOCKER lk(&m_mutex);
    if (!m_terminated)
        m_timerId = nx::vms::common::appContext()->timerManager()->addTimer(
            std::bind(&CrashReporter::scanAndReportByTimer, this, settings),
            std::chrono::milliseconds(SCAN_TIMER_CYCLE));
}

bool CrashReporter::send(const nx::utils::Url& serverApi, const QFileInfo& crash, QSettings* settings)
{
    auto filePath = crash.absoluteFilePath();
    QFile file(filePath);
    file.open(QFile::ReadOnly);
    auto content = file.readAll();
    if (content.size() == 0)
    {
        NX_WARNING(this, lit("Error: %1 is not readable or empty: %2")
                .arg(filePath).arg(file.errorString()));
        return false;
    }

    auto httpClient = nx::network::http::AsyncHttpClient::create(nx::network::ssl::kDefaultCertificateCheck);
    auto report = new ReportData(crash, settings, *this, httpClient.get());
    QObject::connect(httpClient.get(), &nx::network::http::AsyncHttpClient::done,
                    report, &ReportData::finishReport, Qt::DirectConnection);

    httpClient->setCredentials(nx::network::http::PasswordCredentials(
        nx::vms::statistics::kDefaultUser.toStdString(),
        nx::vms::statistics::kDefaultPassword.toStdString()));
    httpClient->setAdditionalHeaders(report->makeHttpHeaders());

    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_activeHttpClient)
    {
        NX_WARNING(this, lit("Another report already is in progress!"));
        return false;
    }

    NX_INFO(this, lit("Send %1 to %2").arg(filePath).arg(serverApi.toString()));

    httpClient->doPost(serverApi, "application/octet-stream", std::move(content));
    m_activeHttpClient = std::move(httpClient);
    return true;
}

ReportData::ReportData(const QFileInfo& crashFile, QSettings* settings,
                       CrashReporter& host, QObject* parent)
    : QObject(parent)
    , m_crashFile(crashFile)
    , m_settings(settings)
    , m_host(host)
{
}

void ReportData::finishReport(nx::network::http::AsyncHttpClientPtr httpClient)
{
    if (!httpClient->hasRequestSucceeded())
    {
        NX_WARNING(this, lit("Sending %1 to %2 has failed")
                .arg(m_crashFile.absoluteFilePath())
                .arg(httpClient->url().toString()));
    }
    else
    {
        NX_DEBUG(this, lit("Report %1 has been sent successfully")
                .arg(m_crashFile.absoluteFilePath()));

        const auto now = qnSyncTime->currentDateTime().toUTC();
        m_settings->setValue(LAST_CRASH, now.toString(Qt::ISODate));
        m_settings->sync();

        QFile::rename(m_crashFile.absoluteFilePath(),
                      m_crashFile.absoluteDir().absoluteFilePath(
                          SENT_PREFIX + m_crashFile.fileName()));
    }

    NX_MUTEX_LOCKER lock(&m_host.m_mutex);
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

    const auto uuidHash = m_host.peerId().toSimpleString().replace(lit("-"), lit(""));
    const auto version = nx::utils::AppInfo::applicationFullVersion();
    const auto systemInfo = nx::vms::api::OsInformation::fromBuildInfo().toString();
    const auto systemRuntime = nx::vms::api::OsInformation::currentSystemRuntime();
    const auto system = lit( "%1 %2" ).arg( systemInfo ).arg( systemRuntime )
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
