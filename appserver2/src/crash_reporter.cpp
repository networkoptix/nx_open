#include "crash_reporter.h"

#include <QDateTime>

#include <api/app_server_connection.h>
#include <api/global_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <common/common_module.h>

#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>

#include <utils/common/app_info.h>
#include <utils/common/scoped_thread_rollback.h>
#include <utils/common/synctime.h>

#include "ec2_thread_pool.h"

static const QString DATE_FORMAT = lit("yyyy-MM-dd_hh-mm-ss");
static const QString SERVER_API_COMMAND = lit("crashserver/api/report");
static const QString LAST_CRASH = lit("statisticsReportLastCrash");
static const QString SENT_PREFIX = lit("sent_");

static const uint SENDING_MIN_INTERVAL = 24 * 60 * 60; /* secs => a day */
static const uint SENDING_MIN_SIZE = 1 * 1024; /* less then 1kb is not informative */
static const uint SENDING_MAX_SIZE = 32 * 1024 * 1024; /* over 30mb is too big */
static const uint SCAN_TIMER_CYCLE = 10 * 60 * 1000; /* every 10 minutes */
static const uint KEEP_LAST_CRASHES = 10;

static QFileInfoList readCrashes(const QString& prefix = QString())
{
    #if defined( _WIN32 )
        const QDir crashDir(QString::fromStdString(win32_exception::getCrashDirectory()));
        const auto crashFilter = prefix + QString::fromStdString(win32_exception::getCrashPattern());
    #elif defined (Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        const QDir crashDir(QString::fromStdString(linux_exception::getCrashDirectory()));
        const auto crashFilter = prefix + QString::fromStdString(linux_exception::getCrashPattern());
    #else
        Q_UNUSED(prefix)
        const QDir crashDir;
        const QString crashFilter;
        return QFileInfoList(); // do nothing. not implemented
    #endif

    NX_LOG(lit("readCrashes: scan %1 for files %2")
           .arg(crashDir.absolutePath()).arg(crashFilter), cl_logDEBUG1);

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

    return std::move(files);
}

namespace ec2 {

CrashReporter::CrashReporter(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_terminated(false)
{
}

CrashReporter::~CrashReporter()
{
    boost::optional<qint64> timerId;
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
        timerId = m_timerId;
    }

    if (timerId)
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(*timerId);

    // wait for the last scanAndReportAsync
    m_activeCollection.cancel();
    m_activeCollection.waitForFinished();

    // cancel async IO
    nx_http::AsyncHttpClientPtr httpClient;
    {
        QnMutexLocker lock(&m_mutex);
        std::swap(httpClient, m_activeHttpClient);
    }
    if (httpClient)
        httpClient->pleaseStopSync();
}

bool CrashReporter::scanAndReport(QSettings* settings)
{
    const auto& globalSettings = commonModule()->globalSettings();

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
        NX_LOGX(lit("Automatic report system is disabled"), cl_logDEBUG1);
        return false;
    }

    const auto now = qnSyncTime->currentDateTime().toUTC();
    const auto lastTime = QDateTime::fromString(
            settings->value(LAST_CRASH, "").toString(), Qt::ISODate);

    if (now < lastTime.addSecs(SENDING_MIN_INTERVAL) &&
        lastTime < now.addSecs(SENDING_MIN_INTERVAL)) // avoid possible long resync problem
    {
        NX_LOGX(lit("Previous crash was reported %1, exit")
                .arg(lastTime.toString(Qt::ISODate)), cl_logDEBUG1);
        return false;
    }

    const QString configApi = globalSettings->statisticsReportServerApi();
    const QString serverApi = configApi.isEmpty() ? Ec2StaticticsReporter::DEFAULT_SERVER_API : configApi;
    const nx::utils::Url url = lit("%1/%2").arg(serverApi).arg(SERVER_API_COMMAND);

    auto crashes = readCrashes();
    while (!crashes.isEmpty())
    {
        const auto& crash = crashes.first();

        if (crash.size() < SENDING_MIN_SIZE)
        {
            QFile::remove(crash.absoluteFilePath());
            NX_LOGX(lit("Remove not informative crash: %1")
                .arg(crash.absolutePath()), cl_logDEBUG2);
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
    QnMutexLocker lock(&m_mutex);

    // This function is not supposed to be called more then once per binary, but anyway:
    if (m_activeCollection.isInProgress())
    {
        NX_LOGX(lit("Previous report is in progress, exit"), cl_logERROR);
        return;
    }

    NX_LOGX(lit("Start new async report"), cl_logINFO);
    m_activeCollection = nx::utils::concurrent::run(Ec2ThreadPool::instance(), [=](){
        // \class nx::utils::concurrent posts a job to \class Ec2ThreadPool rather than create new
        // real thread, we need to reverve a thread to avoid possible deadlock
        QnScopedThreadRollback reservedThread( 1, Ec2ThreadPool::instance() );
        return scanAndReport(settings);
    });
}

void CrashReporter::scanAndReportByTimer(QSettings* settings)
{
    if (nx::utils::AppInfo::applicationVersion().endsWith(lit(".0")))
    {
        NX_LOGX(lm("Sending is disabled for developer builds (buildNumber=0)"), cl_logINFO);
        return;
    }

    scanAndReportAsync(settings);

    QnMutexLocker lk(&m_mutex);
    if (!m_terminated)
        m_timerId = nx::utils::TimerManager::instance()->addTimer(
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
        NX_LOGX(lit("Error: %1 is not readable or empty: %2")
                .arg(filePath).arg(file.errorString()), cl_logWARNING);
        return false;
    }

    auto httpClient = nx_http::AsyncHttpClient::create();
    auto report = new ReportData(crash, settings, *this, httpClient.get());
    QObject::connect(httpClient.get(), &nx_http::AsyncHttpClient::done,
                    report, &ReportData::finishReport, Qt::DirectConnection);

    httpClient->setUserName(Ec2StaticticsReporter::AUTH_USER);
    httpClient->setUserPassword(Ec2StaticticsReporter::AUTH_PASSWORD);
    httpClient->setAdditionalHeaders(report->makeHttpHeaders());

    QnMutexLocker lock(&m_mutex);
    if (m_activeHttpClient)
    {
        NX_LOGX(lit("Another report already is in progress!"), cl_logWARNING);
        return false;
    }

    NX_LOGX(lit("Send %1 to %2").arg(filePath).arg(serverApi.toString()), cl_logINFO);

    httpClient->doPost(serverApi, "application/octet-stream", content);
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

void ReportData::finishReport(nx_http::AsyncHttpClientPtr httpClient)
{
    if (!httpClient->hasRequestSucceeded())
    {
        NX_LOGX(lit("Sending %1 to %2 has failed")
                .arg(m_crashFile.absoluteFilePath())
                .arg(httpClient->url().toString()), cl_logWARNING);
    }
    else
    {
        NX_LOGX(lit("Report %1 has been sent successfully")
                .arg(m_crashFile.absoluteFilePath()), cl_logDEBUG1);

        const auto now = qnSyncTime->currentDateTime().toUTC();
        m_settings->setValue(LAST_CRASH, now.toString(Qt::ISODate));
        m_settings->sync();

        QFile::rename(m_crashFile.absoluteFilePath(),
                      m_crashFile.absoluteDir().absoluteFilePath(
                          SENT_PREFIX + m_crashFile.fileName()));
    }

    QnMutexLocker lock(&m_host.m_mutex);
    NX_ASSERT(!m_host.m_activeHttpClient || m_host.m_activeHttpClient == httpClient);
    m_host.m_activeHttpClient.reset();
}

nx_http::HttpHeaders ReportData::makeHttpHeaders() const
{
    const auto fileName = m_crashFile.fileName();

#if defined( _WIN32 )
    const auto binName = fileName.split(QChar('.')).first(); // remove extension (.exe)
#else
    const auto binName = fileName.split(QChar('_')).first();
#endif

    const auto version = nx::utils::AppInfo::applicationFullVersion();
    const auto systemInfo = QnSystemInformation::currentSystemInformation().toString();
    const auto systemRuntime = QnSystemInformation::currentSystemRuntime();
    const auto system = lit( "%1 %2" ).arg( systemInfo ).arg( systemRuntime )
            .replace( QChar( ' ' ), QChar( '-' ) );

    const auto timestamp = m_crashFile.created().toUTC().toString("yyyy-MM-dd_hh-mm-ss");
    const auto extension = fileName.split(QChar('.')).last();

    QCryptographicHash uuidHash(QCryptographicHash::Sha1);
    uuidHash.addData(m_host.commonModule()->moduleGUID().toByteArray());

    nx_http::HttpHeaders headers;
    headers.insert(std::make_pair("Nx-Binary", binName.toUtf8()));
    headers.insert(std::make_pair("Nx-Uuid-Hash", uuidHash.result().toHex()));
    headers.insert(std::make_pair("Nx-Version", version.toUtf8()));
    headers.insert(std::make_pair("Nx-System", system.toUtf8()));
    headers.insert(std::make_pair("Nx-Timestamp", timestamp.toUtf8()));
    headers.insert(std::make_pair("Nx-Extension", extension.toUtf8()));
    return headers;
}

} // namespace ec2
