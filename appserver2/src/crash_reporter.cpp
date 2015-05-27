#include "crash_reporter.h"

#include <common/systemexcept_linux.h>
#include <common/systemexcept_win32.h>

#include <utils/common/app_info.h>
#include <utils/common/synctime.h>

#include "ec2_thread_pool.h"

#include <QDateTime>
#include <QDebug>

static const QString DATE_FORMAT = lit("yyyy-MM-dd_hh-mm-ss");
static const QString SERVER_API_COMMAND = lit("crashserver/api/report");
static const QString LAST_CRASH = lit("statisticsReportLastCrash");
static const uint SENDING_MIN_INTERVAL = 24 * 60 * 60; /* secs => a day */
static const uint SENDING_MAX_SIZE = 32 * 1024 * 1024; /* 30 mb */

namespace ec2 {

CrashReporter::~CrashReporter()
{
    // wait for the last scanAndReportAsync
    m_activeCollection.cancel();
    m_activeCollection.waitForFinished();

    // cancel async IO
    std::set<nx_http::AsyncHttpClientPtr> httpClients;
    {
        QMutexLocker lock(&m_mutex);
        std::swap(httpClients, m_activeHttpClients);
    }
}

bool CrashReporter::scanAndReport(QnUserResourcePtr admin, QSettings* settings)
{
    if (admin->getProperty(Ec2StaticticsReporter::SR_ALLOWED).toInt() == 0)
    {
        qDebug() << "CrashReporter::scanAndReport: sending is not allowed";
        return false;
    }

    const auto now = qnSyncTime->currentDateTime().toUTC();
    const auto lastTime = QDateTime::fromString(
            settings->value(LAST_CRASH, "").toString(), Qt::ISODate);

    if (now < lastTime.addSecs(SENDING_MIN_INTERVAL) &&
        lastTime < now.addSecs(SENDING_MIN_INTERVAL)) // avoid possible long resync problem
    {
        qDebug() << lit("CrashReporter::scanAndReport: previous crash was reported %1")
            .arg(lastTime.toString(Qt::ISODate));
        return false;
    }

    #if defined( _WIN32 )
        const QDir crashDir(QString::fromStdString(win32_exception::getCrashDirectory()));
        const auto crashFilter = QString::fromStdString(win32_exception::getCrashPattern());
    #elif defined ( __linux__ )
        const QDir crashDir(QString::fromStdString(linux_exception::getCrashDirectory()));
        const auto crashFilter = QString::fromStdString(linux_exception::getCrashPattern());
    #else
        const QDir crashDir;
        const QString crashFilter;
        return; // do nothing
    #endif

    const QString configApi = admin->getProperty(Ec2StaticticsReporter::SR_SERVER_API);
    const QString serverApi = configApi.isEmpty() ? Ec2StaticticsReporter::DEFAULT_SERVER_API : configApi;
    const QUrl url = lit("%1/%2").arg(serverApi).arg(SERVER_API_COMMAND);

    qDebug() << "CrashReporter::scanAndReport:" << crashDir.absolutePath() << "for files:" << crashFilter;
    auto crashes = crashDir.entryInfoList(QStringList() << crashFilter, QDir::Files, QDir::Time);
    while (!crashes.isEmpty())
    {
        const auto& crash = crashes.last();

        // FIXME: figure out what to do with the rest of core dumps and also outdated ones
        if (crash.size() < SENDING_MAX_SIZE)
            return CrashReporter::send(url, crash, settings);

        crashes.pop_back();
    }
    return false;
}

void CrashReporter::scanAndReportAsync(QnUserResourcePtr admin, QSettings* settings)
{
    QMutexLocker lock(&m_mutex);

    // This function is not supposed to be called more then once per binary, but anyway:
    m_activeCollection.waitForFinished();

    m_activeCollection = QnConcurrent::run(Ec2ThreadPool::instance(), [=](){
        return scanAndReport(admin, settings);
    });
}

bool CrashReporter::send(const QUrl& serverApi, const QFileInfo& crash, QSettings* settings)
{
    auto filePath = crash.absoluteFilePath();
    QFile file(filePath);
    file.open(QFile::ReadOnly);
    auto content = file.readAll();
    if (content.size() == 0)
    {
        qDebug() << "CrashReporter::send: Error:" << filePath << "is not readable or empty:" 
                 << file.errorString();
        return false;
    }

    auto httpClient = std::make_shared<nx_http::AsyncHttpClient>();
    auto report = new ReportData(crash, settings, *this, httpClient.get());
    QObject::connect(httpClient.get(), &nx_http::AsyncHttpClient::done,
                    report, &ReportData::finishReport, Qt::DirectConnection);

    httpClient->setUserName(Ec2StaticticsReporter::AUTH_USER);
    httpClient->setUserPassword(Ec2StaticticsReporter::AUTH_PASSWORD);
    httpClient->setAdditionalHeaders(report->makeHttpHeaders());

    QMutexLocker lock(&m_mutex);
    qDebug() << "CrashReporter::send:" << filePath << "to" << serverApi;
    if (httpClient->doPost(serverApi, "application/octet-stream", content))
    {
        m_activeHttpClients.insert(httpClient);
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
    {
        QMutexLocker lock(&m_host.m_mutex);
        m_host.m_activeHttpClients.erase(httpClient);
    }

    if (!httpClient->hasRequestSuccesed())
    {
        qDebug() << "CrashReporter::finishReport: sending" << m_crashFile.absoluteFilePath()
                 << "to" << httpClient->url() << "has failed";
        return;
    }

    const auto now = qnSyncTime->currentDateTime().toUTC();
    QFile::remove(m_crashFile.absoluteFilePath());

    m_settings->setValue(LAST_CRASH, now.toString(Qt::ISODate));
    m_settings->sync();
}

nx_http::HttpHeaders ReportData::makeHttpHeaders() const
{
    auto fileName = m_crashFile.fileName();
#if defined( _WIN32 )
    auto binName = fileName.split(QChar('.')).first(); // remove extension (.exe)
#else
    auto binName = fileName.split(QChar('_')).first();
#endif
    auto version = QnAppInfo::applicationFullVersion();
    auto systemInfo = QnAppInfo::applicationSystemInfo();
    auto timestamp = m_crashFile.created().toUTC().toString("yyyy-MM-dd hh:mm:ss");
    auto extension = fileName.split(QChar('.')).last();

    nx_http::HttpHeaders headers;
    headers.insert(std::make_pair("Nx-Binary", binName.toStdString().c_str()));
    headers.insert(std::make_pair("Nx-Version", version.toStdString().c_str()));
    headers.insert(std::make_pair("Nx-System", systemInfo.toStdString().c_str()));
    headers.insert(std::make_pair("Nx-Timestamp", timestamp.toStdString().c_str()));
    headers.insert(std::make_pair("Nx-Extension", extension.toStdString().c_str()));
    return headers;
}

} // namespace ec2
