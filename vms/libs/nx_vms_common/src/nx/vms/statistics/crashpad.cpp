// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "crashpad.h"

#include <chrono>
#include <string>
#include <thread>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <client/crash_report_database.h>
#include <client/crashpad_client.h>
#include <client/prune_crash_reports.h>
#include <client/settings.h>
#include <util/misc/uuid.h>

#include <nx/build_info.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/os_information.h>
#include <nx/vms/statistics/settings.h>

using namespace std::chrono;

namespace nx::vms::statistics {

namespace {

std::map<std::string, std::string> getAnnotationsForCrash()
{
    std::map<std::string, std::string> annotations;

    annotations.insert({
        "binary",
        QFileInfo(QCoreApplication::applicationFilePath()).baseName().toStdString()});
    annotations.insert({
        "version",
        nx::utils::AppInfo::applicationFullVersion().toStdString()});

    const auto systemInfo = nx::vms::api::OsInformation::fromBuildInfo().toString();
    const auto systemRuntime = nx::vms::api::OsInformation::currentSystemRuntime();
    const auto system = QString("%1 %2").arg(systemInfo).arg(systemRuntime)
        .replace(QChar(' '), QChar('-'));

    annotations.insert({"system", system.toStdString()});

    return annotations;
}

base::FilePath getCrashReportDatabasePath()
{
    static const auto dbName = "crashpad_db";

    QDir crashpadDbDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    crashpadDbDir.mkpath(dbName);
    const QString dbPath = crashpadDbDir.absoluteFilePath(dbName);

    #if defined(Q_OS_WINDOWS)
        return base::FilePath(dbPath.toStdWString());
    #else
        return base::FilePath(dbPath.toStdString());
    #endif
}

// Extracts bundled certificates into a single pem file inside crash database folder.
std::string getPathToCertBundle(const base::FilePath& databasePath)
{
    static const auto certName = "cert_bundle.pem";

    const QDir certParentDir(
        #if defined(Q_OS_WINDOWS)
            QString::fromStdWString(databasePath.value())
        #else
            QString::fromStdString(databasePath.value())
        #endif
    );

    const QString certPath = certParentDir.absoluteFilePath(certName);

    QFile certBundle(certPath);
    if (!certBundle.open(QIODevice::WriteOnly))
    {
        NX_WARNING(NX_SCOPE_TAG, "Cannot open file %1 for writing", certPath);
        return {};
    }

    const QDir folder(":/trusted_certificates");
    for (const auto& fileInfo: folder.entryInfoList({"*.pem"}, QDir::Files))
    {
        const QString fileName = fileInfo.fileName();
        QFile f(folder.absoluteFilePath(fileName));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            NX_WARNING(NX_SCOPE_TAG, "Cannot open file %1", folder.absoluteFilePath(fileName));
            continue;
        }
        certBundle.write(f.readAll());
    }

    return certPath.toStdString();
}

void appendDirToEnvVar(const char* name, const QDir& dir)
{
    QByteArray newValue = qgetenv(name);
    if (!newValue.isEmpty())
        newValue.append(QDir::listSeparator().toLatin1());
    newValue.append(dir.absolutePath().toUtf8());
    qputenv(name, newValue);
}

base::FilePath getCrashHandlerPath()
{
    QDir appDir(QCoreApplication::applicationDirPath());
    auto handlerPath = appDir.absoluteFilePath(
        nx::build_info::isWindows()
            ? "crashpad_handler.exe"
            : "crashpad_handler");

    // On Linux and Android, crash_handler depends on our openssl library.
    if (const bool isLinux = nx::build_info::isLinux(); isLinux || nx::build_info::isAndroid())
    {
        if (isLinux)
            appDir.cd("../lib");
        appendDirToEnvVar("LD_LIBRARY_PATH", appDir);
    }

    #if defined(Q_OS_WINDOWS)
        return base::FilePath(handlerPath.toStdWString());
    #else
        return base::FilePath(handlerPath.toStdString());
    #endif
}

} // namespace

NX_VMS_COMMON_API bool initCrashpad(bool enableUploads)
{
    // Initialize crash reports database and crash handler.
    const base::FilePath databasePath = getCrashReportDatabasePath();

    auto database = crashpad::CrashReportDatabase::Initialize(databasePath);

    database->GetSettings()->SetUploadsEnabled(enableUploads);

    // On all platforms except iOS crash_handler prunes the database periodically, the first prune
    // happens 10 minutes after the application start. We may no be running for that long, so do
    // an initial prune after 20 seconds just in case.
    std::thread(
        [database=std::move(database)]()
        {
            std::this_thread::sleep_for(20s);
            auto pruneCondition = crashpad::PruneCondition::GetDefault();
            const time_t k3Days = 60 * 60 * 24 * 3; //< Same as in crash_handler.
            database->CleanDatabase(k3Days);
            crashpad::PruneCrashReportDatabase(database.get(), pruneCondition.get());
        }).detach();

    const base::FilePath handlerPath = getCrashHandlerPath();

    // Add annotations and authorization header.
    std::map<std::string, std::string> annotations = getAnnotationsForCrash();

    const std::string url = defaultCrashReportApiUrl().toStdString();
    const auto authHeaderValue =
        "Basic " + nx::utils::toBase64(defaultUser() + std::string(":") + defaultPassword());

    std::vector<std::string> extraArguments;

    extraArguments.push_back("--no-rate-limit");

    extraArguments.push_back("--header");
    extraArguments.push_back("Authorization=" + authHeaderValue);

    // Some systems may not have required certificates, so export the bundled ones.
    // This is Linux/Android only because on Windows and macOS/iOS crashpad_handler uses native
    // APIs for networking and can't use a different certificate storage.
    if (nx::build_info::isLinux() || nx::build_info::isAndroid())
    {
        if (auto certPath = getPathToCertBundle(databasePath); !certPath.empty())
            extraArguments.push_back("--cert=" + certPath);
    }

    crashpad::CrashpadClient client;
    #if defined(Q_OS_IOS)
        const bool started = client.StartCrashpadInProcessHandler(
            databasePath,
            url,
            annotations,
            /*callback*/ {},
            {{"Authorization", authHeaderValue}});

        // Because iOS crash handler runs in-process, we do not fully process dumps on crash.
        // It is done here when the application is launched.
        std::thread(
            [annotations=std::move(annotations)]()
            {
                crashpad::CrashpadClient::ProcessIntermediateDumps(annotations);
                crashpad::CrashpadClient::StartProcessingPendingReports();
            }).detach();
    #elif defined(Q_OS_ANDROID)
        const bool started = client.StartHandlerAtCrash(
            handlerPath,
            databasePath,
            /*metrics_dir*/ databasePath,
            url,
            annotations,
            extraArguments);
    #else
        const bool started = client.StartHandler(
            handlerPath,
            databasePath,
            /*metrics_dir*/ databasePath,
            url,
            annotations,
            extraArguments,
            /*restartable*/ true,
            /*asynchronous_start*/ false);
    #endif

    NX_DEBUG(NX_SCOPE_TAG, "Crashpad database: %1 Uploads: %2 Handler: %3 (%4) ",
        databasePath.value().c_str(),
        enableUploads ? "enabled" : "disabled",
        handlerPath.value().c_str(),
        started ? "started" : "failed to start");

    return started;
}

NX_VMS_COMMON_API nx::Uuid getCrashpadClientId()
{
    const base::FilePath databasePath = getCrashReportDatabasePath();
    crashpad::UUID id;
    auto database = crashpad::CrashReportDatabase::Initialize(databasePath);
    if (!NX_ASSERT(database->GetSettings()->GetClientID(&id)))
        return {};

    return nx::Uuid::fromString(id.ToString());
}

} // namespace nx::vms::statistics
