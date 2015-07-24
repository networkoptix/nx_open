#include "crash_reporter.h"

#include <api/app_server_connection.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

#include <common/common_module.h>
#include <common/systemexcept_linux.h>
#include <common/systemexcept_win32.h>

#include <utils/common/app_info.h>
#include <utils/common/log.h>
#include <utils/common/scoped_thread_rollback.h>
#include <utils/common/synctime.h>

#include "ec2_thread_pool.h"

#include <QDateTime>

static const QString DATE_FORMAT = lit("yyyy-MM-dd_hh-mm-ss");
static const QString SERVER_API_COMMAND = lit("crashserver/api/report");
static const QString LAST_CRASH = lit("statisticsReportLastCrash");
static const uint SENDING_MIN_INTERVAL = 24 * 60 * 60; /* secs => a day */
static const uint SENDING_MAX_SIZE = 32 * 1024 * 1024; /* 30 mb */
static const uint SCAN_TIMER_CYCLE = 10 * 60 * 1000; /* every 10 minutes */

static QFileInfoList readCrashes()
{
    #if defined( _WIN32 )
        const QDir crashDir(QString::fromStdString(win32_exception::getCrashDirectory()));
        const auto crashFilter = QString::fromStdString(win32_exception::getCrashPattern());
    #elif defined ( __linux__ )
        const QDir crashDir(QString::fromStdString(linux_exception::getCrashDirectory()));
        const auto crashFilter = QString::fromStdString(linux_exception::getCrashPattern());
    #else
        const QDir crashDir;
        const QString crashFilter;
        return QFileInfoList(); // do nothing. not implemented
    #endif

    NX_LOG(lit("CrashReporter::readCrashes: scan %1 for files %2")
        .arg(crashDir.absolutePath()).arg(crashFilter), cl_logDEBUG1);

    return crashDir.entryInfoList(QStringList() << crashFilter, QDir::Files, QDir::Time);
}

namespace ec2 {

CrashReporter::CrashReporter()
    : m_terminated(false)
{
}

CrashReporter::~CrashReporter()
{
    boost::optional<qint64> timerId;
    {
        QMutexLocker lock(&m_mutex);
        m_terminated = true;
        timerId = m_timerId;
    }

    if (timerId)
        TimerManager::instance()->joinAndDeleteTimer(*timerId);

    // wait for the last scanAndReportAsync
    m_activeCollection.cancel();
    m_activeCollection.waitForFinished();

    // cancel async IO
    nx_http::AsyncHttpClientPtr httpClient;
    {
        QMutexLocker lock(&m_mutex);
        std::swap(httpClient, m_activeHttpClient);
    }
}

bool CrashReporter::scanAndReport(QSettings* settings)
{
    const auto admin = qnResPool->getAdministrator();
    if (!admin) return false;

    const auto msManager = QnAppServerConnectionFactory::getConnection2()->getMediaServerManager();
    if (!Ec2StaticticsReporter::isAllowed(msManager))
    {
        NX_LOG(lit("CrashReporter::scanAndReport: Automatic report system is disabled"),
               cl_logINFO);
        return false;
    }

    const auto now = qnSyncTime->currentDateTime().toUTC();
    const auto lastTime = QDateTime::fromString(
            settings->value(LAST_CRASH, "").toString(), Qt::ISODate);

    if (now < lastTime.addSecs(SENDING_MIN_INTERVAL) &&
        lastTime < now.addSecs(SENDING_MIN_INTERVAL)) // avoid possible long resync problem
    {
        NX_LOG(lit("CrashReporter::scanAndReport: previous crash was reported %1")
            .arg(lastTime.toString(Qt::ISODate)), cl_logDEBUG1);
        return false;
    }

    const QString configApi = admin->getProperty(Ec2StaticticsReporter::SR_SERVER_API);
    const QString serverApi = configApi.isEmpty() ? Ec2StaticticsReporter::DEFAULT_SERVER_API : configApi;
    const QUrl url = lit("%1/%2").arg(serverApi).arg(SERVER_API_COMMAND);

    auto crashes = readCrashes();
    while (!crashes.isEmpty())
    {
        const auto& crash = crashes.first();
        if (crash.size() < SENDING_MAX_SIZE)
            return CrashReporter::send(url, crash, settings);

        QFile::remove(crash.absoluteFilePath());
        crashes.pop_front();
    }

    return false;
}

void CrashReporter::scanAndReportAsync(QSettings* settings)
{
    QMutexLocker lock(&m_mutex);
    m_activeCollection.waitForFinished();

    m_activeCollection = QnConcurrent::run(Ec2ThreadPool::instance(), [=](){
        // \class QnConcurrent posts a job to \class Ec2ThreadPool rather than create new
        // real thread, we need to reverve a thread to avoid possible deadlock
        QnScopedThreadRollback reservedThread( 1, Ec2ThreadPool::instance() );
        return scanAndReport(settings);
    });
}

void CrashReporter::scanAndReportByTimer(QSettings* settings)
{
    scanAndReportAsync(settings);

    QMutexLocker lk(&m_mutex);
    if (!m_terminated)
        m_timerId = TimerManager::instance()->addTimer(
            std::bind(&CrashReporter::scanAndReportByTimer, this, settings), SCAN_TIMER_CYCLE);
}

bool CrashReporter::send(const QUrl& serverApi, const QFileInfo& crash, QSettings* settings)
{
    auto filePath = crash.absoluteFilePath();
    QFile file(filePath);
    file.open(QFile::ReadOnly);
    auto content = file.readAll();
    if (content.size() == 0)
    {
        NX_LOG(lit("CrashReporter::send: Error: %1 is not readable or empty: %2")
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

    QMutexLocker lock(&m_mutex);
    if (m_activeHttpClient)
    {
        NX_LOG(lit("CrashReporter::send: another report is in progress!"), cl_logWARNING);
        return false;
    }

    NX_LOG(lit("CrashReporter::send: %1 to %2")
           .arg(filePath).arg(serverApi.toString()), cl_logINFO);

    if (httpClient->doPost(serverApi, "application/octet-stream", content))
    {
        m_activeHttpClient = std::move(httpClient);
        return true;
    }

    return false;
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
    if (!httpClient->hasRequestSuccesed())
    {
        NX_LOG(lit("CrashReporter::finishReport: sending %1 to %2 has failed")
               .arg(m_crashFile.absoluteFilePath())
               .arg(httpClient->url().toString()), cl_logWARNING);
        return;
    }

    NX_LOG(lit("CrashReporter::finishReport: crash %1 has been sent successfully")
           .arg(m_crashFile.absoluteFilePath()), cl_logDEBUG1);

    const auto now = qnSyncTime->currentDateTime().toUTC();
    m_settings->setValue(LAST_CRASH, now.toString(Qt::ISODate));
    m_settings->sync();

    for (const auto crash : readCrashes())
        QFile::remove(crash.absoluteFilePath());

    QMutexLocker lock(&m_host.m_mutex);
    Q_ASSERT(!m_host.m_activeHttpClient || m_host.m_activeHttpClient == httpClient);
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

    const auto version = QnAppInfo::applicationFullVersion();
    const auto systemInfo = QnAppInfo::applicationSystemInfo();
    const auto timestamp = m_crashFile.created().toUTC().toString("yyyy-MM-dd_hh-mm-ss");
    const auto extension = fileName.split(QChar('.')).last();

    QCryptographicHash uuidHash(QCryptographicHash::Sha1);
    uuidHash.addData(qnCommon->moduleGUID().toByteArray());

    nx_http::HttpHeaders headers;
    headers.insert(std::make_pair("Nx-Binary", binName.toUtf8()));
    headers.insert(std::make_pair("Nx-Uuid-Hash", uuidHash.result().toHex()));
    headers.insert(std::make_pair("Nx-Version", version.toUtf8()));
    headers.insert(std::make_pair("Nx-System", systemInfo.toUtf8()));
    headers.insert(std::make_pair("Nx-Timestamp", timestamp.toUtf8()));
    headers.insert(std::make_pair("Nx-Extension", extension.toUtf8()));
    return headers;
}

} // namespace ec2
