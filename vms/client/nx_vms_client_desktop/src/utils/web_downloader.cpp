// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_downloader.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtWebEngineCore/QWebEngineDownloadRequest>

#include <client/client_message_processor.h>
#include <common/common_module.h>
#include <core/resource/file_processor.h>
#include <core/resource/resource.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/command_action.h>
#include <nx/vms/client/desktop/common/widgets/webview_controller.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <platform/environment.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/message_box.h>

#include "reply_read_timeout.h"

using namespace nx::vms::client::desktop::utils;

namespace {

static const qint64 kQuietStopTimeoutMs = 10000;
static const auto kDataReadTimeout = std::chrono::milliseconds(60000);

QString getUniqueFilePath(const QString& path)
{
    static constexpr auto kNumTries = 100;
    static constexpr auto kNameSuffix = " (%1)";

    const auto fileInfo = QFileInfo(path);
    if (!fileInfo.exists())
        return path;

    // Split the file into 2 parts - dot+extension, and everything else.
    // For example, "path/file.tar.gz" becomes "path/file"+".tar.gz", while
    // "path/file" (note lack of extension) becomes "path/file"+"".
    QString secondPart = fileInfo.completeSuffix();
    QString firstPart;
    if (!secondPart.isEmpty())
    {
        secondPart = "." + secondPart;
        firstPart = path.left(path.size() - secondPart.size());
    }
    else
    {
        firstPart = path;
    }

    // Try with an ever-increasing number suffix, until we've reached a file
    // that does not yet exist.
    for (int i = 1; i <= kNumTries; i++)
    {
        // Construct the new file name by adding the unique number between the
        // first and second part.
        const auto suffix = QString(kNameSuffix).arg(i);
        const auto newPath = firstPart + suffix + secondPart;

        // If no file exists with the new name, return it.
        if (!QFileInfo::exists(newPath))
            return newPath;
    }

    // All names are taken - bail out and return the original.
    return path;
}

QString speedToText(qint64 bytesRead, qint64 elapsedMs)
{
    static constexpr qint64 kBitsPerByte = 8;

    static constexpr auto kMsPerSec = 1000;
    static constexpr qreal kBitsPerKb = 1000.0;
    static constexpr qreal kKbPerMb = 1000.0;
    static constexpr qreal kMbPerGb = 1000.0;

    if (elapsedMs == 0)
        elapsedMs = 1;

    QString speedLabel = "Kbps";
    qreal speed = (bytesRead * kBitsPerByte) / (kBitsPerKb * elapsedMs / kMsPerSec);

    if (speed >= kKbPerMb)
    {
        speedLabel = "Mbps";
        speed /= kKbPerMb;
    }

    if (speed >= kMbPerGb)
    {
        speedLabel = "Gbps";
        speed /= kMbPerGb;
    }

    return QString("%1 %2").arg(speed, 0, 'f', 1).arg(speedLabel);
}

QString fixupIllegalFileName(const QString& filename)
{
    // Characters considered illegal/dangerous in a filename.
    static const QRegularExpression kIllegalCharsRx("[\\/:\\*\\?\"<>\\|]");

    QString result = filename;
    result.replace(kIllegalCharsRx, "_");
    return result;
}

} // namespace

WebDownloader::WebDownloader(
    WindowContext* windowContext,
    QObject* parent,
    const QString& filePath,
    QObject* item)
    :
    base_type(parent),
    WindowContextAware(windowContext),
    m_fileInfo(filePath),
    m_item(item)
{
    startDownload();
}

WebDownloader::~WebDownloader()
{
    windowContext()->localNotificationsManager()->remove(m_notificationId);

    if (m_state == State::Completed)
        return;

    if (m_item)
    {
        QMetaObject::invokeMethod(m_item, "cancel");
        return;
    }
}

bool WebDownloader::download(QObject* item, WindowContext* context)
{
    const auto suggestedName =
        fixupIllegalFileName(item->property("downloadFileName").toString());

    // Determine parent widget for showing the Save dialog.
    const auto view = item->property("view").value<QObject*>();

    QWidget* parentWidget = nullptr;

    if (view && view->parent())
    {
        const auto controller =
            view->parent()->property("controller").value<WebViewController*>();
        if (controller)
            parentWidget = controller->containerWidget();
    }

    if (!parentWidget)
        parentWidget = context->mainWindowWidget();

    QString path = selectFile(suggestedName, parentWidget);
    if (path.isEmpty())
        return false;

    QFileInfo file(path);
    item->setProperty("downloadDirectory", file.absolutePath());
    item->setProperty("downloadFileName", file.fileName());

    // Downloader will be destroyed when current system connection is broken.
    new WebDownloader(context, context->localNotificationsManager(), path, item);

    return true;
}

QString WebDownloader::selectFile(const QString& suggestedName, QWidget* widget)
{
    auto lastDir = appContext()->localSettings()->lastDownloadDir();
    if ((lastDir.isEmpty() || !QDir(lastDir).exists())
        && !appContext()->localSettings()->mediaFolders().isEmpty())
    {
        lastDir = appContext()->localSettings()->mediaFolders().first();
    }

    auto filePath = getUniqueFilePath(QDir(lastDir).filePath(suggestedName));

    // Loop until we have a writable file path.
    while (true)
    {
        // Request file path from the user.
        QnCustomFileDialog saveDialog(widget,
            tr("Save File As..."),
            filePath,
            QnCustomFileDialog::createFilter(QnCustomFileDialog::kAllFilesFilter));
        saveDialog.setFileMode(QFileDialog::AnyFile);
        saveDialog.setAcceptMode(QFileDialog::AcceptSave);

        if (!saveDialog.exec())
            return QString();

        filePath = saveDialog.selectedFile();

        if (filePath.isEmpty())
            return QString();

        // Check if its writable.
        const auto fileInfo = QFileInfo(filePath);
        filePath = fileInfo.absoluteFilePath();

        auto file = std::make_unique<QFile>();
        file->setFileName(filePath);
        if (file->open(QIODevice::WriteOnly))
        {
            file->close();
            file->remove();
            appContext()->localSettings()->lastDownloadDir = fileInfo.absolutePath();
            return filePath;
        }

        // Unable to write to selected file - show error message.
        QnMessageBox messageBox(widget);
        messageBox.setIcon(QnMessageBoxIcon::Critical);
        if (fileInfo.exists())
        {
            messageBox.setText(tr("Failed to overwrite file"));
            messageBox.setInformativeText(QDir::toNativeSeparators(filePath));
        }
        else
        {
            messageBox.setText(tr("Failed to save file"));
            messageBox.setInformativeText(
                tr("%1 folder is blocked for writing.")
                    .arg(QDir::toNativeSeparators(fileInfo.absolutePath())));
        }
        messageBox.setStandardButtons(QDialogButtonBox::Ok);
        messageBox.exec();
    }

    NX_ASSERT(false, "Unreachable");
    return QString();
}

void WebDownloader::startDownload()
{
    auto notificationsManager = windowContext()->localNotificationsManager();
    m_notificationId = notificationsManager->addProgress(
        tr("Downloading file..."), m_fileInfo.fileName(), /*cancellable*/ true);
    setState(State::Downloading);

    auto action = CommandActionPtr(new CommandAction(tr("Open Containing Folder")));
    connect(action.data(), &QAction::triggered, this,
        [this]()
        {
            QnEnvironment::showInGraphicalShell(m_fileInfo.absoluteFilePath());
        });

    notificationsManager->setAction(m_notificationId, action);

    connect(notificationsManager,
        &workbench::LocalNotificationsManager::removed,
        this,
        [this](const QnUuid& notificationId) {
            if (notificationId != m_notificationId)
                return;
            this->deleteLater();
        });

    auto messageProcessor = system()->clientMessageProcessor();
    auto deleteIfDisconnected =
        [this, messageProcessor]
        {
            if (messageProcessor->connectionStatus()->state() == QnConnectionState::Disconnected)
                this->deleteLater();
        };

    connect(messageProcessor->connectionStatus(),
        &QnClientConnectionStatus::stateChanged,
        this,
        deleteIfDisconnected);

    connect(notificationsManager,
        &workbench::LocalNotificationsManager::interactionRequested,
        this,
        [this](const QnUuid& notificationId) {
            if (notificationId != m_notificationId)
                return;

            if (m_state != State::Completed)
                return;

            const QStringList files{m_fileInfo.absoluteFilePath()};

            // TODO: #sivanov Use local files system context in the future.
            const auto resources = QnFileProcessor::createResourcesForFiles(
                QnFileProcessor::findAcceptedFiles(files),
                system()->resourcePool());

            if (resources.isEmpty())
                return;

            menu()->trigger(menu::DropResourcesAction, resources);
        });

    connect(notificationsManager,
        &workbench::LocalNotificationsManager::cancelRequested,
        this,
        [this](const QnUuid& notificationId) {
            if (notificationId != m_notificationId)
                return;
            cancel();
        });

    connect(m_item, SIGNAL(receivedBytesChanged()), this, SLOT(onReceivedBytesChanged()));
    connect(m_item, SIGNAL(stateChanged(QWebEngineDownloadRequest::DownloadState)),
        this, SLOT(onStateChanged(QWebEngineDownloadRequest::DownloadState)));
    DownloadItemReadTimeout::set(m_item, kDataReadTimeout);
    QMetaObject::invokeMethod(m_item, "accept");
}

void WebDownloader::cancel()
{
    if (m_state != State::Downloading)
    {
        this->deleteLater();
        return;
    }

    if (m_downloadTimer.hasExpired(kQuietStopTimeoutMs))
    {
        QnMessageBox messageBox(mainWindowWidget());
        messageBox.setIcon(QnMessageBoxIcon::Question);
        messageBox.setText(tr("Stop file downloading?"));
        messageBox.setInformativeText(m_fileInfo.fileName());
        messageBox.setStandardButtons(QDialogButtonBox::Cancel);
        messageBox.addCustomButton(QnMessageBoxCustomButton::Stop,
            QDialogButtonBox::AcceptRole,
            Qn::ButtonAccent::Warning);

        if (messageBox.exec() == QDialogButtonBox::Cancel)
            return;
    }
    m_cancelRequested = true;

    this->deleteLater();
}

void WebDownloader::setState(State state)
{
    if (state == m_state)
        return;

    const auto prevState = m_state;
    m_state = state;

    auto progressManager = windowContext()->localNotificationsManager();

    switch (state)
    {
        case State::Downloading:
            NX_ASSERT(prevState == State::Init);
            progressManager->setProgress(
                m_notificationId, ProgressState::indefinite);
            m_downloadTimer.start();
            break;
        case State::Completed:
            NX_ASSERT(prevState == State::Downloading);
            progressManager->setProgress(m_notificationId,
                ProgressState::completed);
            progressManager->setTitle(m_notificationId, tr("File downloaded"));
            break;
        case State::Failed:
            NX_ASSERT(prevState == State::Downloading);
            progressManager->setProgress(
                m_notificationId, ProgressState::failed);
            progressManager->setTitle(m_notificationId, tr("File downloading failed"));
            break;
        default:
            NX_ASSERT(false && "Invalid state change");
            break;
    }
}

void WebDownloader::onReceivedBytesChanged()
{
    onDownloadProgress(
        m_item->property("receivedBytes").value<qint64>(),
        m_item->property("totalBytes").value<qint64>());
}

void WebDownloader::onStateChanged(QWebEngineDownloadRequest::DownloadState state)
{
    switch (state)
    {
        case QWebEngineDownloadRequest::DownloadRequested:
        case QWebEngineDownloadRequest::DownloadInProgress:
            setState(State::Downloading);
            break;
        case QWebEngineDownloadRequest::DownloadCompleted:
            setState(State::Completed);
            break;
        default:
            if (!m_cancelRequested)
                setState(State::Failed);
            break;
    }
}

void WebDownloader::onDownloadProgress(qint64 bytesRead, qint64 bytesTotal)
{
    auto progressManager = windowContext()->localNotificationsManager();
    if (m_state != State::Downloading)
        return;

    if (m_initElapsed == 0)
    {
        m_initBytesRead = bytesRead;
        m_initElapsed = m_downloadTimer.elapsed();
    }

    const QString speed = speedToText(
        bytesRead - m_initBytesRead,
        m_downloadTimer.elapsed() - m_initElapsed);

    progressManager->setProgress(m_notificationId, static_cast<qreal>(bytesRead) / bytesTotal);
    progressManager->setProgressFormat(m_notificationId, QString("%1, %p%").arg(speed));
}
