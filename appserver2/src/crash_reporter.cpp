#include "crash_reporter.h"

#include <common/systemexcept_linux.h>
#include <common/systemexcept_win32.h>

#include <utils/common/app_info.h>
#include <utils/common/concurrent.h>

#include "ec2_thread_pool.h"

#include <QDateTime>
#include <QDebug>

static const QString DATE_FORMAT = lit("yyyy-MM-dd_hh-mm-ss");
static const QString SERVER_API_COMMAND = lit("crash/api/report");

namespace ec2 {

void CrashReporter::scanAndReport(QnUserResourcePtr admin)
{
    if (admin->getProperty(Ec2StaticticsReporter::SR_ALLOWED).toInt() == 0)
    {
        qDebug() << "CrashReporter::scanAndReport: sending is not allowed";
        return;
    }

    #if defined( _WIN32 )
        QDir crashDir(win32_exception::getCrashDirectory());
        const auto crashFilter = win32_exception::getCrashPattern();
    #elif defined ( __linux__ )
        QDir crashDir(linux_exception::getCrashDirectory());
        const auto crashFilter = linux_exception::getCrashPattern();
    #else
        QDir crashDir;
        QString crashFilter;
        return; // do nothing
    #endif

    const QString configApi = admin->getProperty(Ec2StaticticsReporter::SR_SERVER_API);
    const QString serverApi = configApi.isEmpty() ? Ec2StaticticsReporter::DEFAULT_SERVER_API : configApi;
    const QUrl url = lit("%1/%2").arg(serverApi).arg(SERVER_API_COMMAND);
    const bool auth = admin->getProperty(Ec2StaticticsReporter::SR_SERVER_NO_AUTH).toInt() == 0;

    // all crashes are going to be send in different threads
    qDebug() << "CrashReporter::scanAndReport:" << crashDir.absolutePath() << "for files:" << crashFilter;
    for (const auto& crash : crashDir.entryInfoList(QStringList() << crashFilter))
        CrashReporter::send(url, crash, auth);
}

void CrashReporter::scanAndReportAsync(QnUserResourcePtr admin)
{
    QnConcurrent::run(Ec2ThreadPool::instance(),
                      std::bind(&CrashReporter::scanAndReport, admin));
}

void CrashReporter::send(const QUrl& serverApi, const QFileInfo& crash, bool auth)
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
    auto report = new CrashReporter(crash, httpClient.get());
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
    if (httpClient->hasRequestSuccesed())
        QFile::remove(m_crashFile.absoluteFilePath());
    else
        qDebug() << "CrashReporter::finishReport:" << httpClient->url() << "has failed";

    // no reason to keep client alive any more
    c_activeHttpClients.erase(httpClient);
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

CrashReporter::CrashReporter(const QFileInfo& crashFile, QObject* parent)
    : QObject(parent)
    , m_crashFile(crashFile)
{
}

std::set<nx_http::AsyncHttpClientPtr> CrashReporter::c_activeHttpClients;

} // namespace ec2
