#include "crash_reporter.h"

#include <common/systemexcept_linux.h>
#include <common/systemexcept_win32.h>

#include <utils/common/app_info.h>
#include <utils/common/concurrent.h>

#include "ec2_thread_pool.h"

#include <QDateTime>
#include <QDebug>

static const QString DATE_FORMAT = lit("yyyy-MM-dd_hh-mm-ss");
static const QString SERVER_API_COMMAND = lit("crashserver/api/report");
static const QString LAST_CRASH = lit("statisticsReportLastCrash");
static const uint SENDING_MIN_INTERVAL = 24 * 60 * 60; /* secs => a day */

namespace ec2 {

void CrashReporter::scanAndReport(QnUserResourcePtr admin, QSettings* settings)
{
    if (admin->getProperty(Ec2StaticticsReporter::SR_ALLOWED).toInt() == 0)
    {
        qDebug() << "CrashReporter::scanAndReport: sending is not allowed";
        return;
    }
    
    const auto lastTime = QDateTime::fromString(
            settings->value(LAST_CRASH, "").toString(), DATE_FORMAT);
    if (QDateTime::currentDateTime() < lastTime.addSecs(SENDING_MIN_INTERVAL))
    {
        qDebug() << lit("CrashReporter::scanAndReport: previous crash was reported %1")
            .arg(lastTime.toString(DATE_FORMAT));
        return;
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
    const bool auth = admin->getProperty(Ec2StaticticsReporter::SR_SERVER_NO_AUTH).toInt() == 0;

    // all crashes are going to be send in different threads
    qDebug() << "CrashReporter::scanAndReport:" << crashDir.absolutePath() << "for files:" << crashFilter;
    const auto crashes = crashDir.entryInfoList(QStringList() << crashFilter, QDir::Files, QDir::Time);
    if (crashes.size()) CrashReporter::send(url, crashes.last(), auth, settings);
}

void CrashReporter::scanAndReportAsync(QnUserResourcePtr admin, QSettings* settings)
{
    QnConcurrent::run(Ec2ThreadPool::instance(),
                      std::bind(&CrashReporter::scanAndReport, admin, settings));
}

void CrashReporter::send(const QUrl& serverApi, const QFileInfo& crash, bool auth, QSettings* settings)
{
    auto filePath = crash.absoluteFilePath();
    QFile file(filePath);
    file.open(QFile::ReadOnly);
    auto content = file.readAll();
    if (content.size() == 0)
    {
        qDebug() << "CrashReporter::send: Error:" << filePath << "is not readable or empty:" 
                 << file.errorString();
        return;
    }

    auto httpClient = std::make_shared<nx_http::AsyncHttpClient>();
    auto report = new CrashReporter(crash, settings, httpClient.get());
    connect(httpClient.get(), &nx_http::AsyncHttpClient::done,
            report, &CrashReporter::finishReport, Qt::DirectConnection);

    if (auth)
    {
        httpClient->setUserName(Ec2StaticticsReporter::AUTH_USER);
        httpClient->setUserPassword(Ec2StaticticsReporter::AUTH_PASSWORD);
    }

    // keep client alive until the job is done
    c_activeHttpClients.insert(httpClient);

    qDebug() << "CrashReporter::send:" << filePath << "to" << serverApi;
    httpClient->doPost(serverApi, "application/octet-stream",
                       content, report->makeHttpHeaders());
}

void CrashReporter::finishReport(nx_http::AsyncHttpClientPtr httpClient) const
{
    // no reason to keep client alive any more
    c_activeHttpClients.erase(httpClient);

    if (!httpClient->hasRequestSuccesed())
    {
        qDebug() << "CrashReporter::finishReport: sending" << m_crashFile.absoluteFilePath()
                 << "to" << httpClient->url() << "has failed";
        return;
    }

    QFile::remove(m_crashFile.absoluteFilePath());
    m_settings->setValue(LAST_CRASH, QDateTime::currentDateTime().toString(DATE_FORMAT));
    m_settings->sync();
}

nx_http::HttpHeaders CrashReporter::makeHttpHeaders() const
{
    auto fileName = m_crashFile.fileName();
#if defined( _WIN32 )
    auto binName = fileName.split(QChar('.')).first(); // remove extension (.exe)
#else
    auto binName = fileName.split(QChar('_')).first();
#endif
    auto version = QnAppInfo::applicationFullVersion();
    auto systemInfo = QnAppInfo::applicationSystemInfo();
    auto timestamp = m_crashFile.created().toString(DATE_FORMAT);
    auto extension = fileName.split(QChar('.')).last();

    nx_http::HttpHeaders headers;
    headers.insert(std::make_pair("Nx-Binary", binName.toStdString().c_str()));
    headers.insert(std::make_pair("Nx-Version", version.toStdString().c_str()));
    headers.insert(std::make_pair("Nx-System", systemInfo.toStdString().c_str()));
    headers.insert(std::make_pair("Nx-Timestamp", timestamp.toStdString().c_str()));
    headers.insert(std::make_pair("Nx-Extension", extension.toStdString().c_str()));
    return headers;
}

CrashReporter::CrashReporter(const QFileInfo& crashFile, QSettings* settings, QObject* parent)
    : QObject(parent)
    , m_crashFile(crashFile)
    , m_settings(settings)
{
}

std::set<nx_http::AsyncHttpClientPtr> CrashReporter::c_activeHttpClients;

} // namespace ec2
