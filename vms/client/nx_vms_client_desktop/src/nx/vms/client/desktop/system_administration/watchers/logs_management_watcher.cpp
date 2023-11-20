// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logs_management_watcher.h"

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtGui/QAction>

#include <api/common_message_processor.h>
#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/http/async_file_downloader.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_settings.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_administration/models/logs_management_model.h>
#include <nx/vms/client/desktop/system_administration/widgets/log_settings_dialog.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/common/network/abstract_certificate_verifier.h>
#include <nx/zip/extractor.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

namespace {

const QString kFailedSuffix{"_failed"};
const QString kNormalSuffix{""};
const QString kPartialSuffix{"_partial"};

bool needLogLevelWarningFor(const LogsManagementWatcher::UnitPtr& unit)
{
    auto settings = unit->settings();
    return settings && settings->mainLog.primaryLevel > LogsManagementWatcher::defaultLogLevel();
}

QString shortList(const QList<QnMediaServerResourcePtr>& servers)
{
    QString result;

    for (int i = 0; i < qMin(3, servers.size()); ++i)
    {
        auto info = QnResourceDisplayInfo(servers[i]);
        result += nx::format("<b>%1</b> (%2) <br/>", info.name(), info.host());
    }

    if (servers.size() > 3)
        result += LogsManagementWatcher::tr("... and %n more", "", servers.size() - 3);

    return result;
}

LogsManagementWatcher::State watcherState(Qt::CheckState state)
{
    return state == Qt::Unchecked
        ? LogsManagementWatcher::State::empty
        : LogsManagementWatcher::State::hasSelection;
}

QString makeFileName(QnMediaServerResourcePtr server)
{
    const auto name = server ? server->getName() : "client";
    const auto id = server ? server->getId().toSimpleString() : QnUuid().toSimpleString();
    const auto time = QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss");
    return server
        ? nx::format("%1_%2_%3%4.zip", name, id, time)
        : nx::format("%1_%2%3.zip", name, time);
}

nx::vms::api::ServerLogSettings loadClientSettings()
{
    using namespace nx::utils::log;

    nx::vms::api::ServerLogSettings result;
    result.mainLog.primaryLevel = levelFromString(appContext()->localSettings()->logLevel());

    result.maxVolumeSizeB = appContext()->localSettings()->maxLogVolumeSizeB();
    result.maxFileSizeB = appContext()->localSettings()->maxLogFileSizeB();
    result.maxFileTimePeriodS = appContext()->localSettings()->maxLogFileTimePeriodS();
    result.archivingEnabled = appContext()->localSettings()->logArchivingEnabled();

    return result;
}

void storeAndApplyClientSettings(nx::vms::api::ServerLogSettings settings)
{
    using namespace nx::log;
    using namespace nx::utils::log;

    const auto level = settings.mainLog.primaryLevel;
    appContext()->localSettings()->logLevel = LogsManagementModel::logLevelName(level);
    appContext()->localSettings()->maxLogVolumeSizeB = settings.maxVolumeSizeB;
    appContext()->localSettings()->maxLogFileSizeB = settings.maxFileSizeB;
    appContext()->localSettings()->maxLogFileTimePeriodS = settings.maxFileTimePeriodS;
    appContext()->localSettings()->logArchivingEnabled = settings.archivingEnabled;

    LoggerSettings loggerSettings;
    loggerSettings.maxVolumeSizeB = settings.maxVolumeSizeB;
    loggerSettings.maxFileSizeB = settings.maxFileSizeB;
    loggerSettings.maxFileTimePeriodS = settings.maxFileTimePeriodS;
    loggerSettings.archivingEnabled = settings.archivingEnabled;
    loggerSettings.level.primary = level;

    // Initialize as a set in order to remove duplicates.
    const std::set<std::shared_ptr<AbstractLogger>> loggers{
        mainLogger(),
        getExactLogger(kHttpTag),
        getExactLogger(kSystemTag)};

    for (auto logger: loggers)
    {
        logger->setSettings(loggerSettings);
    }
}

} // namespace

ClientLogCollector::ClientLogCollector(const QString& target, QObject *parent):
    base_type(parent),
    m_target(target)
{
}

ClientLogCollector::~ClientLogCollector()
{
    pleaseStop();

    // This class instance is created and destroyed in the GUI thread. Therefore it must be
    // destroyed in time. If not, there is a problem with the thread loop that should be fixed.
    constexpr int kDelay = 500; //< milliseconds.
    NX_ASSERT(wait(kDelay), "Log collection thread has not stopped");
}

void ClientLogCollector::pleaseStop()
{
    m_cancelled = true;
}

void ClientLogCollector::run()
{
    using namespace nx::utils::log;

    QFileInfo fileInfo(m_target);
    const auto& tempTarget = fileInfo.dir().filePath(
        fileInfo.baseName() + kPartialSuffix + '.' + fileInfo.completeSuffix());

    auto file = std::make_unique<QFile>(tempTarget);
    if (!file->open(QIODevice::ReadWrite))
    {
        NX_ERROR(this, "Could not open file for writing: %1", tempTarget);
        emit error();
        return;
    }

    QuaZip archive(&*file);
    archive.setZip64Enabled(true);
    if (!archive.open(QuaZip::Mode::mdCreate))
    {
        NX_ERROR(this, "Could not create zip archive: %1", archive.getZipError());
        emit error();
        return;
    }

    // TODO: support customized logs.
    const auto name = "client_log";

    for (int rotation = 0; !m_cancelled && rotation <= kMaxLogRotation; rotation++)
    {
        NX_VERBOSE(this, "Archiving %1 for %2", (rotation == 0)
            ? "current log file" : NX_FMT("rotated log file %1", rotation), name);

        auto logger = mainLogger();
        std::optional<QString> baseFileName = logger->filePath();

        if (!baseFileName)
        {
            NX_ERROR(this, "Log file for %1 is stderr", name);
            emit error();
            return;
        }

        for (const auto extension: {File::Extension::log, File::Extension::zip})
        {
            const auto path = File::makeFileName(*baseFileName, rotation, extension);
            if (!QFile::exists(path))
            {
                NX_VERBOSE(this, "Log file for %1 does not exist at the path %2, skipping it", name, path);
                continue;
            }

            static constexpr int kBufferSize = 64 * 1024;
            uint64_t currentSize = 0;
            uint64_t fileSize = 0;
            if (rotation && extension == File::Extension::zip)
            {
                QuaZip rotatedArchive(path);
                if (!rotatedArchive.open(QuaZip::Mode::mdUnzip))
                {
                    NX_ERROR(this, "Could not open zipped log file for %1 at the path %2", name, path);
                    emit error();
                    return;
                }
                if (!rotatedArchive.goToFirstFile())
                {
                    NX_ERROR(this, "Could not find a log file for %1 inside the archive at the path %2: %3",
                        name, path, rotatedArchive.getZipError());
                    emit error();
                    return;
                }

                int method = Z_DEFLATED, level = Z_DEFAULT_COMPRESSION;
                QuaZipFile deflatedLog(&rotatedArchive);
                // Open for reading in raw mode to avoid uncompressing.
                if (!deflatedLog.open(QIODevice::ReadOnly, &method, &level, true))
                {
                    NX_ERROR(this, "Could not open the log file for %1 inside the archive at the path %2: %3",
                        name, path, deflatedLog.getZipError());
                    emit error();
                    return;
                }

                QuaZipFileInfo64 info;
                rotatedArchive.getCurrentFileInfo(&info);

                QFileInfo fileName(path);
                QuaZipNewInfo newInfo(fileName.fileName().replace(toQString(extension),
                    toQString(File::Extension::log)), path);
                newInfo.uncompressedSize = info.uncompressedSize;

                QuaZipFile zip(&archive);
                // Open for writing in raw mode to avoid compressing.
                if (!zip.open(QIODevice::WriteOnly, newInfo, NULL, info.crc, method, level, true))
                {
                    NX_ERROR(this, "Could not add the log file for %1 to the archive: %2",
                        name, zip.getZipError());
                    emit error();
                    return;
                }

                fileSize = fileName.size();
                while (!m_cancelled
                    && deflatedLog.pos() < deflatedLog.size()
                    && deflatedLog.getZipError() == 0 && zip.getZipError() == 0)
                {
                    const auto buf = deflatedLog.read(kBufferSize);
                    emit progressChanged((currentSize += kBufferSize) * 100.0 / fileSize);
                    if (!buf.isEmpty())
                        zip.write(buf);
                }
                zip.close();

                if (deflatedLog.getZipError() != 0)
                {
                    NX_ERROR(this, "Error while reading the log file for %1 at the path %2: %3",
                        name, path, deflatedLog.getZipError());
                    emit error();
                    return;
                }
                if (zip.getZipError() != 0)
                {
                    NX_ERROR(this, "Error while zipping the log file for %1 at the path %2: %3",
                        name, path, zip.getZipError());
                    emit error();
                    return;
                }
            }
            else
            {
                QFile textLog(path);
                if (!textLog.open(QIODevice::ReadOnly))
                {
                    NX_ERROR(this, "Could not open the log file for %1 at path %2", name, path);
                    emit error();
                    return;
                }

                QuaZipFile zip(&archive);
                if (!zip.open(QIODevice::WriteOnly, QuaZipNewInfo(File::makeBaseFileName(path), path)))
                {
                    NX_ERROR(this, "Could not add the log file for %1 to the archive: %2",
                        name, zip.getZipError());
                    emit error();
                    return;
                }

                fileSize = textLog.size();
                while (!m_cancelled && !textLog.atEnd()
                    && textLog.error() == QFileDevice::NoError && zip.getZipError() == 0)
                {
                    const auto buf = textLog.read(kBufferSize);
                    emit progressChanged((currentSize += kBufferSize) * 100.0 / fileSize);
                    if (!buf.isEmpty())
                        zip.write(buf);
                }
                zip.close();

                if (textLog.error() != QFileDevice::NoError)
                {
                    NX_ERROR(this, "Error while reading the log file for %1 at the path %2: %3",
                            name, path, textLog.error());
                    emit error();
                    return;
                }
                if (zip.getZipError() != 0)
                {
                    NX_DEBUG(this, "Error while zipping the log file for %1 at the path %2: %3",
                        name, path, zip.getZipError());
                    emit error();
                    return;
                }
            }
        }
    }

    archive.close();

    if (QFile::rename(tempTarget, m_target))
    {
        emit success();
    }
    else
    {
        NX_DEBUG(this, "Error renaming temporary log file %1 to %2", tempTarget, m_target);
        emit error();
    }
}

struct LogsManagementWatcher::Unit::Private
{
    static std::shared_ptr<Unit> createClientUnit()
    {
        return std::make_shared<Unit>();
    }

    static std::shared_ptr<Unit> createServerUnit(QnMediaServerResourcePtr server)
    {
        auto unit = std::make_shared<LogsManagementWatcher::Unit>();
        unit->d->m_server = server;
        return unit;
    }

    QnUuid id() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_server ? m_server->getId() : QnUuid();
    }

    bool isChecked() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_checked;
    }

    void setChecked(bool isChecked)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_checked = isChecked;
    }

    QnMediaServerResourcePtr server() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_server;
    }

    std::optional<nx::vms::api::ServerLogSettings> settings() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_settings;
    }

    void setSettings(const std::optional<nx::vms::api::ServerLogSettings>& settings)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_settings = settings;
    }

    DownloadState state() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_state;
    }

    ErrorType errorType() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_errorType;
    }

    void setState(Unit::DownloadState state)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_state = state;
    }

    void startClientDownload(
        const QString& folder,
        std::function<void()> callback)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        NX_ASSERT(m_state == DownloadState::none || m_state == DownloadState::error);

        QDir dir(folder);
        auto sourceFilePath = dir.absoluteFilePath(makeFileName(m_server));
        // Immediately make the file with the "_partial" suffix, so as not to do an unnecessary
        // re-opening of the file after renaming.
        m_currentFilePath = sourceFilePath.arg(kPartialSuffix);
        auto file = std::make_shared<QFile>(m_currentFilePath);
        if (!file->open(QIODevice::WriteOnly))
        {
            m_state = DownloadState::error;
            m_errorType = ErrorType::local;
            lock.unlock();
            if (callback)
                callback();
            return;
        }

        file->close();

        m_collector.reset(new ClientLogCollector(dir.absoluteFilePath(m_currentFilePath)));
        connect(m_collector.get(), &ClientLogCollector::success,
            [this, sourceFilePath, callback]
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                m_state = DownloadState::complete;

                lock.unlock();
                renameFileForCurrentState(sourceFilePath);
                if (callback)
                    callback();
            });
        connect(m_collector.get(), &ClientLogCollector::error,
            [this, sourceFilePath, callback]
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                m_state = DownloadState::error;

                lock.unlock();
                renameFileForCurrentState(sourceFilePath);
                if (callback)
                    callback();
            });

        m_collector->start();
        m_state = DownloadState::loading;

    }

    void startServerDownload(
        const QString& folder,
        const nx::utils::Url& apiUrl,
        const network::http::Credentials& credentials,
        nx::network::ssl::AdapterFunc adapterFunc,
        std::function<void()> callback)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        NX_ASSERT(m_state == DownloadState::none || m_state == DownloadState::error);

        m_downloader = std::make_unique<nx::network::http::AsyncFileDownloader>(adapterFunc);

        m_downloader->setCredentials(credentials);
        m_downloader->setTimeouts(nx::network::http::AsyncClient::kInfiniteTimeouts);

        auto serverId = m_server->getId().toSimpleString();
        nx::utils::Url url = apiUrl;
        url.setPath(QString("/rest/v2/servers/%1/logArchive").arg(serverId));

        QDir dir(folder);
        auto sourceFilePath = dir.absoluteFilePath(makeFileName(m_server));
        // Immediately make the file with the "_partial" suffix, so as not to do an unnecessary
        // re-opening of the file after renaming.
        m_currentFilePath = sourceFilePath.arg(kPartialSuffix);
        auto file = std::make_shared<QFile>(m_currentFilePath);
        if (!file->open(QIODevice::WriteOnly))
        {
            m_state = DownloadState::error;
            m_errorType = ErrorType::local;
            lock.unlock();
            if (callback)
                callback();
            return;
        }

        m_bytesLoaded = 0;
        m_fileSize = {};

        m_initElapsed = 0;
        m_initBytesLoaded = 0;
        m_timer.start();

        m_downloader->setOnResponseReceived(
            [this, sourceFilePath, callback](std::optional<size_t> size)
            {
                NX_MUTEX_LOCKER lock(&m_mutex);

                if (!m_downloader->hasRequestSucceeded())
                {
                    lock.unlock();
                    m_downloader->pleaseStopSync();
                    lock.relock();
                    m_state = DownloadState::error;
                    fixZipFileIfNeeded(m_currentFilePath);
                }
                else
                {
                    m_fileSize = size;
                    m_state = DownloadState::loading;
                }

                lock.unlock();
                renameFileForCurrentState(sourceFilePath);
                if (callback)
                    callback();
            });

        m_downloader->setOnProgressHasBeenMade(
            [this, callback](nx::Buffer&& buffer, std::optional<double> progress)
            {
                NX_MUTEX_LOCKER lock(&m_mutex);

                m_bytesLoaded += buffer.size();

                if (m_initElapsed == 0)
                {
                    m_initElapsed = m_timer.elapsed();
                    m_initBytesLoaded = m_bytesLoaded;
                }

                lock.unlock();
                if (callback)
                    callback();
            });

        m_downloader->setOnDone(
            [this, sourceFilePath, callback]
            {
                NX_MUTEX_LOCKER lock(&m_mutex);

                m_state = m_downloader->failed() ? DownloadState::error : DownloadState::complete;

                lock.unlock();
                renameFileForCurrentState(sourceFilePath);
                if (callback)
                    callback();
            });

        m_state = DownloadState::pending;
        m_downloader->start(url, file);
    }

    void fixZipFileIfNeeded(const QString& filePath)
    {
        using namespace nx::zip;
        auto dirPath = QFileInfo(filePath).absoluteDir().absolutePath();
        auto extractor = std::make_unique<Extractor>(filePath, dirPath);
        if (extractor->tryOpen() != Extractor::Ok && QFile::resize(filePath, 0))
            extractor->createEmptyZipFile();
    }

    void stopDownload()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        if (m_collector)
        {
            m_collector.reset();
        }
        if (m_downloader)
        {
            // Downloader can receive some data and try to lock the mutex before it actually stops.
            lock.unlock();
            m_downloader->pleaseStopSync();
            lock.relock();
            m_downloader.reset();
        }
        m_state = DownloadState::none;
    }

    double speed() const //< Bytes per second.
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        if (m_state != DownloadState::loading)
            return 0;

        if (m_initElapsed == 0)
            return 0;

        return 1000. * (m_bytesLoaded - m_initBytesLoaded) / (m_timer.elapsed() - m_initElapsed);
    }

    double progress() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        switch (m_state)
        {
            case DownloadState::none:
            case DownloadState::pending:
                return {};

            case DownloadState::loading:
            {
                if (!m_fileSize)
                    return {};

                return 1. * m_bytesLoaded / *m_fileSize;
            }

            case DownloadState::complete:
            case DownloadState::error:
                return 1;
        }

        NX_ASSERT(false);
        return {};
    }

    size_t bytesLoaded() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        switch (m_state)
        {
            case DownloadState::none:
            case DownloadState::pending:
                return 0;

            case DownloadState::loading:
            case DownloadState::complete:
            case DownloadState::error:
                return m_bytesLoaded;
        }
    }

    void renameFileForCurrentState(const QString& sourceFilePath)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        QString newFilePath;
        switch (m_state)
        {
            case DownloadState::loading:
                newFilePath = sourceFilePath.arg(kPartialSuffix);
                break;
            case DownloadState::error:
                newFilePath = sourceFilePath.arg(kFailedSuffix);
                break;
            default:
                newFilePath = sourceFilePath.arg(kNormalSuffix);
                break;
        }

        QFile::rename(m_currentFilePath, newFilePath);
        m_currentFilePath = newFilePath;
    }

private:
    mutable nx::Mutex m_mutex;
    QnMediaServerResourcePtr m_server;

    bool m_checked{false};
    QString m_currentFilePath;
    DownloadState m_state{DownloadState::none};
    ErrorType m_errorType{ErrorType::remote};
    std::optional<nx::vms::api::ServerLogSettings> m_settings;

    std::unique_ptr<ClientLogCollector> m_collector;
    std::unique_ptr<nx::network::http::AsyncFileDownloader> m_downloader;

    size_t m_bytesLoaded{0};
    std::optional<size_t> m_fileSize;

    QElapsedTimer m_timer;
    size_t m_initElapsed{0};
    size_t m_initBytesLoaded{0};

};

QnUuid LogsManagementWatcher::Unit::id() const
{
    return d->id();
}

bool LogsManagementWatcher::Unit::isChecked() const
{
    return d->isChecked();
}

QnMediaServerResourcePtr LogsManagementWatcher::Unit::server() const
{
    return d->server();
}

std::optional<nx::vms::api::ServerLogSettings> LogsManagementWatcher::Unit::settings() const
{
    return d->settings();
}

LogsManagementWatcher::Unit::DownloadState LogsManagementWatcher::Unit::state() const
{
    return d->state();
}

bool LogsManagementWatcher::Unit::errorIsLocal() const
{
    return d->errorType() == Unit::ErrorType::local;
}

LogsManagementWatcher::Unit::Private* LogsManagementWatcher::Unit::data()
{
    return d.get();
}

LogsManagementWatcher::Unit::Unit():
    d(new Private())
{
}

//-------------------------------------------------------------------------------------------------

struct LogsManagementWatcher::Private
{
    Private(LogsManagementWatcher* q): q(q), api(new Api(q))
    {
    }

    mutable nx::Mutex mutex;
    LogsManagementWatcher* const q;

    QString path;
    State state{State::empty};
    bool cancelled{false};
    bool updatesEnabled{false};

    LogsManagementUnitPtr client;
    QList<LogsManagementUnitPtr> servers;

    QPointer<workbench::LocalNotificationsManager> notificationManager;
    QnUuid clientLogLevelWarning;
    QnUuid serverLogLevelWarning;
    QnUuid logsDownloadNotification;

    double speed{0};
    double progress{0};

    QList<LogsManagementUnitPtr> items() const
    {
        QList<LogsManagementUnitPtr> result;
        result << client;
        result << servers;
        return result;
    }

    QList<LogsManagementUnitPtr> checkedItems() const
    {
        QList<LogsManagementUnitPtr> result;

        if (client->isChecked())
            result << client;

        for (auto& server: servers)
        {
            if (server->isChecked())
                result << server;
        }

        return result;
    }

    Qt::CheckState selectionState() const
    {
        const auto allItems = items();
        const auto selectedItems = checkedItems();

        return selectedItems.isEmpty()
            ? Qt::Unchecked
            : (selectedItems.size() == allItems.size())
                ? Qt::Checked
                : Qt::PartiallyChecked;
    }

    void initNotificationManager()
    {
        if (notificationManager)
            return;

        const auto context = appContext()->mainWindowContext();
        notificationManager = context->localNotificationsManager();;

        q->connect(
            notificationManager, &workbench::LocalNotificationsManager::cancelRequested, q,
            [this](const QnUuid& notificationId)
            {
                NX_MUTEX_LOCKER lock(&mutex);
                hideNotification(notificationId);
            });

        q->connect(
            notificationManager, &workbench::LocalNotificationsManager::interactionRequested, q,
            [this](const QnUuid& notificationId)
            {
                if (notificationId == clientLogLevelWarning
                    || notificationId == serverLogLevelWarning
                    || notificationId == logsDownloadNotification)
                {
                    appContext()->mainWindowContext()->menu()->trigger(menu::LogsManagementAction);
                }
            });
    }

    void updateClientLogLevelWarning()
    {
        if (!NX_ASSERT(notificationManager))
            return;

        if (needLogLevelWarningFor(client))
        {
            if (clientLogLevelWarning.isNull())
            {
                clientLogLevelWarning = notificationManager->add(
                    tr("Debug logging is enabled on the Client"),
                    {},
                    true);

                notificationManager->setLevel(clientLogLevelWarning,
                    QnNotificationLevel::Value::ImportantNotification);

                notificationManager->setTooltip(clientLogLevelWarning,
                    tr("Debug logging is enabled, resulting in degraded system performance."));
            }
        }
        else
        {
            if (clientLogLevelWarning.isNull())
                return;

            notificationManager->remove(clientLogLevelWarning);
            clientLogLevelWarning = {};
        }
    }

    void updateServerLogLevelWarning()
    {
        if (!NX_ASSERT(notificationManager))
            return;

        QList<QnMediaServerResourcePtr> resList;
        for (auto& s: servers)
        {
            if (needLogLevelWarningFor(s))
                resList << s->server();
        }

        if (!resList.isEmpty())
        {
            if (serverLogLevelWarning.isNull())
            {
                serverLogLevelWarning = notificationManager->add({}, {}, true);
                notificationManager->setLevel(serverLogLevelWarning,
                    QnNotificationLevel::Value::ImportantNotification);
            }

            notificationManager->setTitle(serverLogLevelWarning,
                tr("Debug logging is enabled on %n Servers", "", resList.size()));

            notificationManager->setAdditionalText(serverLogLevelWarning, shortList(resList));

            notificationManager->setTooltip(serverLogLevelWarning,
                tr("Debug logging is enabled, resulting in degraded system performance."));
        }
        else
        {
            if (serverLogLevelWarning.isNull())
                return;

            notificationManager->remove(serverLogLevelWarning);
            serverLogLevelWarning = {};
        }
    }

    void updateLogsDownloadNotification()
    {
        if (!NX_ASSERT(notificationManager))
            return;

        bool show = ((state != State::empty) && (state != State::hasSelection)) || cancelled;

        if (show)
        {
            if (logsDownloadNotification.isNull())
            {
                logsDownloadNotification = notificationManager->add({}, {}, true);
                notificationManager->setLevel(logsDownloadNotification,
                    QnNotificationLevel::Value::ImportantNotification);
            }

            QList<UnitPtr> units;
            QList<QnMediaServerResourcePtr> resources;
            QList<Unit::DownloadState> filter;

            switch (state)
            {
                case State::loading:
                    filter << Unit::DownloadState::pending
                        << Unit::DownloadState::loading
                        << Unit::DownloadState::complete
                        << Unit::DownloadState::error;
                    break;

                case State::finished:
                    filter << Unit::DownloadState::complete;
                    break;

                case State::hasErrors:
                case State::hasLocalErrors:
                    filter << Unit::DownloadState::error;
                    break;
            }

            for (auto& server: servers)
            {
                if (filter.contains(server->data()->state()))
                {
                    units << server;
                    resources << server->server();
                }
            }

            QnNotificationLevel::Value level;
            QString title;
            QString additionalText;
            std::optional<ProgressState> progress;
            switch (state)
            {
                case State::empty:
                case State::hasSelection:
                {
                    NX_ASSERT(cancelled, "Internal logic error");

                    level = QnNotificationLevel::Value::ImportantNotification;

                    title = tr("Logs downloading canceled");

                    break;
                }

                case State::loading:
                {
                    level = QnNotificationLevel::Value::ImportantNotification;

                    title = speed > 0 ? tr("Downloading file...") : tr("Pending download...");

                    additionalText = nx::format("<b>%1</b> <br/> %2",
                        tr("%n Servers", "", units.size()),
                        "0Mbps"); //TODO: #spanasenko Speed & time.

                    progress = {this->progress};

                    break;
                }

                case State::finished:
                {
                    level = QnNotificationLevel::Value::ImportantNotification;

                    title = tr("Logs downloaded");

                    additionalText = nx::format("<b>%1</b>",
                        tr("%n Servers", "", units.size()));

                    progress = {ProgressState::completed};

                    break;
                }

                case State::hasErrors:
                case State::hasLocalErrors:
                {
                    level = QnNotificationLevel::Value::ImportantNotification;

                    title = tr("Logs downloading failed");

                    additionalText = shortList(resources);

                    progress = {ProgressState::failed};

                    break;
                }
            }

            notificationManager->setLevel(logsDownloadNotification, level);
            notificationManager->setTitle(logsDownloadNotification, title);
            notificationManager->setAdditionalText(logsDownloadNotification, additionalText);
            notificationManager->setProgress(logsDownloadNotification, progress);

        }
        else
        {
            if (logsDownloadNotification.isNull())
                return;

            notificationManager->remove(logsDownloadNotification);
            logsDownloadNotification = {};
        }
    }

    void hideNotification(const QnUuid& notificationId)
    {
        if (notificationId == clientLogLevelWarning)
        {
            notificationManager->remove(clientLogLevelWarning);
            clientLogLevelWarning = {};
        }
        else if (notificationId == serverLogLevelWarning)
        {
            notificationManager->remove(serverLogLevelWarning);
            serverLogLevelWarning = {};
        }
        else if (notificationId == logsDownloadNotification)
        {
            notificationManager->remove(logsDownloadNotification);
            logsDownloadNotification = {};
            //TODO: #spanasenko Cancel download.
        }
    }

    // A helper struct with long-running functions that should be called without locking the mutex.
    // These functions are encapsulated into a separate struct to ensure that they don't access any
    // Private fields.
    struct Api
    {
        Api(LogsManagementWatcher* q): q(q) {}
        LogsManagementWatcher* const q;

        void loadInitialSettings()
        {
            auto callback = nx::utils::guarded(q,
                [this](bool success, rest::Handle requestId, QByteArray result, auto headers)
                {
                    if (!success)
                        return;

                    auto [list, deserializationResult] = nx::reflect::json::deserialize<
                        std::map<std::string, nx::vms::api::ServerLogSettings>>(
                            result.toStdString());

                    if (!NX_ASSERT(deserializationResult.success))
                        return;

                    for (const auto& [key, settings]: list)
                    {
                        q->onReceivedServerLogSettings(settings.getId(), settings);
                    }
                });

            q->connection()->serverApi()->getRawResult(
                "/rest/v2/servers/*/logSettings",
                {},
                callback,
                q->thread()
            );
        }

        void loadServerSettings(const QnUuid& serverId)
        {
            auto callback = nx::utils::guarded(q,
                [this](bool success, rest::Handle requestId, QByteArray result, auto headers)
                {
                    if (!success)
                        return;

                    auto [settings, deserializationResult] = nx::reflect::json::deserialize<
                        nx::vms::api::ServerLogSettings>(result.toStdString());

                    if (!NX_ASSERT(deserializationResult.success))
                        return;

                    q->onReceivedServerLogSettings(settings.getId(), settings);
                });

            q->connection()->serverApi()->getRawResult(
                QString("/rest/v2/servers/%1/logSettings").arg(serverId.toString()),
                {},
                callback,
                q->thread()
            );
        }

        bool storeServerSettings(
            nx::vms::common::SessionTokenHelperPtr helper,
            LogsManagementUnitPtr server,
            const ConfigurableLogSettings& settings)
        {
            auto existing = server->settings();
            if (!existing)
                return false; //TODO: #spanasenko Report an error.

            auto oldSettings = *existing;
            auto newSettings = settings.applyTo(*existing);
            server->data()->setSettings(newSettings);

            auto callback = nx::utils::guarded(q,
                [this, server, oldSettings](
                    bool success,
                    rest::Handle requestId,
                    rest::ServerConnection::ErrorOrEmpty result)
                {
                    if (!success)
                        server->data()->setSettings(oldSettings);

                    q->onSentServerLogSettings(server->id(), success);
                    //TODO: #spanasenko Report an error.
                });

            q->connection()->serverApi()->putServerLogSettings(
                helper,
                server->id(),
                newSettings,
                callback,
                q->thread()
            );

            return true;
        }
    };
    QScopedPointer<Api> api;
};

LogsManagementWatcher::LogsManagementWatcher(SystemContext* context, QObject* parent):
    base_type(parent),
    SystemContextAware(context),
    d(new Private(this))
{
    d->client = Unit::Private::createClientUnit();
    d->client->data()->setSettings(loadClientSettings());

    connect(resourcePool(), &QnResourcePool::resourcesAdded, this,
        [this](const QnResourceList& resources)
        {
            QList<QnUuid> addedServers;

            {
                NX_MUTEX_LOCKER lock(&d->mutex);

                for (const auto& resource: resources)
                {
                    if (auto server = resource.dynamicCast<QnMediaServerResource>())
                    {
                        auto unit = Unit::Private::createServerUnit(server);
                        d->servers << unit;
                        addedServers << server->getId();

                        auto notify = [this, unit]{ emit itemsChanged({unit}); };
                        connect(server.get(), &QnResource::statusChanged, this, notify);
                    }
                }
            }

            if (d->updatesEnabled)
            {
                for (auto& serverId: addedServers)
                    d->api->loadServerSettings(serverId);
            }

            if (!addedServers.isEmpty())
                emit itemListChanged();
        });

    connect(resourcePool(), &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            bool removed = false;

            {
                NX_MUTEX_LOCKER lock(&d->mutex);

                for (const auto& resource: resources)
                {
                    if (auto server = resource.dynamicCast<QnMediaServerResource>())
                    {

                        auto it = std::find_if(d->servers.begin(), d->servers.end(),
                            [id=server->getId()](auto& unit){ return unit->id() == id; });

                        if (it == d->servers.end())
                            continue;

                        server->disconnect(this);
                        d->servers.erase(it);
                        removed = true;
                    }
                }
            }

            if (removed)
                emit itemListChanged();
        });

    qRegisterMetaType<State>();
}

LogsManagementWatcher::~LogsManagementWatcher()
{
}

void LogsManagementWatcher::setMessageProcessor(QnCommonMessageProcessor* messageProcessor)
{
    if (messageProcessor)
    {
        connect(
            messageProcessor,
            &QnCommonMessageProcessor::initialResourcesReceived,
            this,
            [this]
            {
                {
                    NX_MUTEX_LOCKER lock(&d->mutex);
                    d->path = "";
                    d->state = State::empty;

                    // Reinit client unit.
                    d->client->data()->setChecked(false);

                    d->initNotificationManager();
                    d->updateClientLogLevelWarning();
                    d->updateServerLogLevelWarning();
                }

                emit stateChanged(State::empty);
                emit selectionChanged(Qt::Unchecked);
                emit itemListChanged();
                d->api->loadInitialSettings();
            });
    }
}

QList<LogsManagementUnitPtr> LogsManagementWatcher::items() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->items();
}

QList<LogsManagementUnitPtr> LogsManagementWatcher::checkedItems() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->checkedItems();
}

QString LogsManagementWatcher::path() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->path;
}

void LogsManagementWatcher::setItemIsChecked(LogsManagementUnitPtr item, bool checked)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->state == State::empty || d->state == State::hasSelection);

    if (!NX_ASSERT(item))
        return;

    // Update item state.
    item->data()->setChecked(checked);

    // Evaluate the new state.
    const auto selectionState = d->selectionState();
    const auto oldState = d->state;
    const auto newState = watcherState(selectionState);
    d->state = newState;

    // Emit signals.
    lock.unlock();
    if (oldState != newState)
        emit stateChanged(newState);
    emit itemsChanged({item});
    emit selectionChanged(selectionState);
}

void LogsManagementWatcher::setAllItemsAreChecked(bool checked)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->state == State::empty || d->state == State::hasSelection);

    QList<UnitPtr> changedItems;
    for (auto& item: d->items())
    {
        if (auto server = item->server();
            checked && server && !server->isOnline())
        {
            continue;
        }

        item->data()->setChecked(checked);
        changedItems << item;
    }

    // Evaluate the new state.
    const auto selectionState = d->selectionState();
    const auto oldState = d->state;
    const auto newState = watcherState(selectionState);
    d->state = newState;

    // Emit signals.
    lock.unlock();
    if (oldState != newState)
        emit stateChanged(newState);
    emit itemsChanged(changedItems);
    emit selectionChanged(selectionState);
}

Qt::CheckState LogsManagementWatcher::itemsCheckState() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->selectionState();
}

void LogsManagementWatcher::startDownload(const QString& path)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->state == State::hasSelection);
    const auto newState = State::loading;

    d->path = path;
    auto units = d->checkedItems();
    d->state = newState;
    d->cancelled = false;

    lock.unlock();

    emit stateChanged(newState);
    emit progressChanged(0);

    for (auto& unit: units)
    {
        if (unit->server())
            downloadServerLogs(path, unit);
        else
            downloadClientLogs(path, unit);
    }
}

void LogsManagementWatcher::cancelDownload()
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->state == State::loading);
    const auto newState = watcherState(d->selectionState());

    d->client->data()->stopDownload();
    for (auto server: d->servers)
    {
        server->data()->stopDownload();
    }

    d->path = "";
    d->state = newState;
    d->cancelled = true;

    lock.unlock();
    emit stateChanged(newState);
}

void LogsManagementWatcher::restartFailed()
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->state == State::hasLocalErrors || d->state == State::hasErrors);
    const auto newState = State::loading;

    auto path = d->path;
    QList<UnitPtr> unitsToRestart;

    if (d->client->state() == Unit::DownloadState::error)
        unitsToRestart << d->client;

    for (auto server: d->servers)
    {
        if (server->state() == Unit::DownloadState::error)
            unitsToRestart << server;
    }

    d->state = newState;
    d->cancelled = false;

    lock.unlock();

    emit stateChanged(newState);
    emit progressChanged(0);

    for (auto& unit: unitsToRestart)
    {
        if (unit->server())
            downloadServerLogs(path, unit);
        else
            downloadClientLogs(path, unit);
    }
}

void LogsManagementWatcher::completeDownload()
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->state == State::finished || d->state == State::hasLocalErrors
        || d->state == State::hasErrors);
    const auto newState = watcherState(d->selectionState());

    //TODO: #spanasenko Clean-up.
    d->client->data()->setState(Unit::DownloadState::none);
    for (auto server: d->servers)
    {
        server->data()->setState(Unit::DownloadState::none);
    }

    d->path = "";
    d->state = newState;

    lock.unlock();
    emit stateChanged(newState);
}

void LogsManagementWatcher::setUpdatesEnabled(bool enabled)
{
    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        d->updatesEnabled = enabled;
    }

    if (enabled)
        d->api->loadInitialSettings();
}

void LogsManagementWatcher::applySettings(
    const ConfigurableLogSettings& settings,
    QWidget* parentWidget)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->state == State::hasSelection);

    std::optional<nx::vms::api::ServerLogSettings> newClientSettings;
    if (d->client->isChecked())
    {
        auto existing = d->client->settings();
        if (!NX_ASSERT(existing))
            return;

        d->client->data()->setSettings(settings.applyTo(*existing));
        d->updateClientLogLevelWarning();

        // Store value to apply it when the mutex is released.
        newClientSettings = d->client->settings();
    }

    QList<UnitPtr> serversToStore;
    for (auto server: d->servers)
    {
        if (server->isChecked())
            serversToStore << server;
    }

    lock.unlock();

    if (newClientSettings)
        storeAndApplyClientSettings(*newClientSettings);

    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        parentWidget,
        tr("Apply Settings"),
        tr("Enter your account password"),
        tr("Apply"),
        FreshSessionTokenHelper::ActionType::updateSettings);

    for (auto server: serversToStore)
    {
        d->api->storeServerSettings(sessionTokenHelper, server, settings);
    }
}

nx::utils::log::Level LogsManagementWatcher::defaultLogLevel()
{
    // We don't use the constant from logs_settings.h because it is build-type denepdent.
    return nx::utils::log::Level::info;
}

nx::vms::api::ServerLogSettings LogsManagementWatcher::clientLogSettings()
{
    return loadClientSettings();
}

LogsManagementUnitPtr LogsManagementWatcher::clientUnit() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->client;
}

void LogsManagementWatcher::storeClientSettings(const ConfigurableLogSettings& settings)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    auto existing = d->client->settings();
    if (!NX_ASSERT(existing))
        return;

    d->client->data()->setSettings(settings.applyTo(*existing));
    const auto newClientSettings = d->client->settings();

    if (d->notificationManager)
        d->updateClientLogLevelWarning();

    lock.unlock();
    storeAndApplyClientSettings(*newClientSettings);
}

LogsManagementWatcher::State LogsManagementWatcher::state() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->state;
}

void LogsManagementWatcher::onReceivedServerLogSettings(
    const QnUuid& serverId,
    const nx::vms::api::ServerLogSettings& settings)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    for (auto server: d->servers)
    {
        if (server->id() == serverId)
        {
            server->data()->setSettings(settings);

            if (needLogLevelWarningFor(server))
                d->updateServerLogLevelWarning();

            return;
        }
    }
}

void LogsManagementWatcher::onSentServerLogSettings(
    const QnUuid& serverId,
    bool success)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    d->updateServerLogLevelWarning();
}

void LogsManagementWatcher::downloadClientLogs(const QString& folder, LogsManagementUnitPtr unit)
{
    unit->data()->startClientDownload(
        folder,
        [this]{ executeInThread(thread(), [this]{ updateDownloadState(); }); });
}

void LogsManagementWatcher::downloadServerLogs(const QString& folder, LogsManagementUnitPtr unit)
{
    unit->data()->startServerDownload(
        folder,
        currentServer()->getApiUrl(),
        connection()->credentials(),
        systemContext()->certificateVerifier()->makeAdapterFunc(currentServerId(), currentServer()->getApiUrl()),
        [this]{ executeInThread(thread(), [this]{ updateDownloadState(); }); });
}

void LogsManagementWatcher::updateDownloadState()
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    int loadingCount = 0, successCount = 0, errorCount = 0, localErrorCount = 0;

    double totalProgress = 0;
    double totalSpeed = 0;

    auto updateCounters =
        [&](const LogsManagementUnitPtr& unit)
        {
            using State = Unit::DownloadState;
            switch (unit->state())
            {
                case State::pending:
                case State::loading:
                    loadingCount++;
                    break;
                case State::complete:
                    successCount++;
                    break;
                case State::error:
                {
                    errorCount++;
                    if (unit->errorIsLocal())
                        localErrorCount++;

                    break;
            }
            }
        };

    auto updateProgress =
        [&](const LogsManagementUnitPtr& unit)
        {
            totalProgress += unit->data()->progress();
            totalSpeed += unit->data()->speed();
        };

    updateCounters(d->client);
    updateProgress(d->client);

    for (auto server: d->servers)
    {
        updateCounters(server);
        updateProgress(server);
    }

    auto totalCount = loadingCount + successCount + errorCount;

    auto newState = d->state;
    double progress = 0;
    if (totalCount)
    {
        if (loadingCount)
            newState = State::loading;
        else if (localErrorCount)
            newState = State::hasLocalErrors;
        else if (errorCount)
            newState = State::hasErrors;
        else
            newState = State::finished;

        d->speed = totalSpeed;
        d->progress = progress = totalProgress / totalCount;
    }
    else
    {
        d->speed = 0;
        d->progress = 0;
    }

    bool changed = (d->state != newState);
    d->state = newState;

    d->updateLogsDownloadNotification();

    lock.unlock();
    if (changed)
        emit stateChanged(newState);

    emit progressChanged(progress);
}

} // namespace nx::vms::client::desktop
