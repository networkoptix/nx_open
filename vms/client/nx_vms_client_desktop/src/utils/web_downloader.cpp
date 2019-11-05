#include "web_downloader.h"
#include "reply_read_timeout.h"

#include <QtCore/QDir>
#include <QtWidgets/QAction>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtWebEngineWidgets/QWebEngineDownloadItem>

#include <platform/environment.h>

#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <common/common_module.h>

#include <core/resource/file_processor.h>
#include <core/resource/resource.h>
#include <client/client_settings.h>
#include <client/client_message_processor.h>

#include <ui/workbench/workbench_context.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/custom_file_dialog.h>

#include <nx/vms/client/desktop/common/utils/command_action.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/workbench/extensions/workbench_progress_manager.h>

using WorkbenchProgressManager = nx::vms::client::desktop::WorkbenchProgressManager;
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

} // namespace

WebDownloader::WebDownloader(
    QObject* parent,
    const QString& filePath,
    QObject* item)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_fileInfo(filePath),
    m_item(item)
{
    startDownload();
}

class ContextHelper: public QnWorkbenchContextAware
{
    using base_type = QnWorkbenchContextAware;

public:
    ContextHelper(QObject* parent): base_type(parent) {}

    QWidget* windowWidget() const { return mainWindowWidget(); }
};

WebDownloader::~WebDownloader()
{
    context()->instance<WorkbenchProgressManager>()->remove(m_activityId);

    if (m_state == State::Completed)
        return;

    if (m_item)
    {
        QMetaObject::invokeMethod(m_item, "cancel");
        return;
    }
}

bool WebDownloader::download(QObject* item, QnWorkbenchContext* context)
{
    const auto suggestedName = QFileInfo(item->property("path").toString()).fileName();

    ContextHelper contextHelper(context);

    QString path = selectFile(suggestedName, contextHelper.windowWidget());
    if (path.isEmpty())
        return false;

    item->setProperty("path", path);

    new WebDownloader(context->instance<WorkbenchProgressManager>(), path, item);

    return true;
}

QString WebDownloader::selectFile(const QString& suggestedName, QWidget* widget)
{
    auto lastDir = qnSettings->lastDownloadDir();
    if (lastDir.isEmpty() || !QDir(lastDir).exists())
        lastDir = qnSettings->mediaFolder();

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
            qnSettings->setLastDownloadDir(fileInfo.absolutePath());
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
    auto progressManager = context()->instance<WorkbenchProgressManager>();
    m_activityId = progressManager->add(
        tr("Downloading file..."), m_fileInfo.fileName(), /*cancellable*/ true);
    setState(State::Downloading);

    auto action = CommandActionPtr(new CommandAction(tr("Open Containing Folder")));
    connect(action.data(), &QAction::triggered, this,
        [this]()
        {
            QnEnvironment::showInGraphicalShell(m_fileInfo.absoluteFilePath());
        });

    progressManager->setAction(m_activityId, action);

    connect(progressManager,
        &WorkbenchProgressManager::removed,
        this,
        [this](const QnUuid& activityId) {
            if (activityId != m_activityId)
                return;
            this->deleteLater();
        });

    auto deleteIfDisconnected = [this] {
        if (qnClientMessageProcessor->connectionStatus()->state()
            == QnConnectionState::Disconnected)
        {
            this->deleteLater();
        }
    };

    connect(qnClientMessageProcessor->connectionStatus(),
        &QnClientConnectionStatus::stateChanged,
        this,
        deleteIfDisconnected);

    connect(progressManager,
        &WorkbenchProgressManager::interactionRequested,
        this,
        [this](const QnUuid& activityId) {
            if (activityId != m_activityId)
                return;

            if (m_state != State::Completed)
                return;

            const QStringList files{m_fileInfo.absoluteFilePath()};

            const auto resources = QnFileProcessor::createResourcesForFiles(
                QnFileProcessor::findAcceptedFiles(files));

            if (resources.isEmpty())
                return;

            context()->menu()->trigger(ui::action::DropResourcesAction, resources);
        });

    connect(progressManager,
        &WorkbenchProgressManager::cancelRequested,
        this,
        [this](const QnUuid& activityId) {
            if (activityId != m_activityId)
                return;
            cancel();
        });

    connect(m_item, SIGNAL(receivedBytesChanged()), this, SLOT(onReceivedBytesChanged()));
    connect(m_item, SIGNAL(stateChanged()), this, SLOT(onStateChanged()));
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

    auto progressManager = context()->instance<WorkbenchProgressManager>();

    switch (state)
    {
        case State::Downloading:
            NX_ASSERT(prevState == State::Init);
            progressManager->setProgress(
                m_activityId, WorkbenchProgressManager::kIndefiniteProgressValue);
            m_downloadTimer.start();
            break;
        case State::Completed:
            NX_ASSERT(prevState == State::Downloading);
            progressManager->setProgress(
                m_activityId, WorkbenchProgressManager::kCompletedProgressValue);
            progressManager->setTitle(m_activityId, tr("File downloaded"));
            break;
        case State::Failed:
            NX_ASSERT(prevState == State::Downloading);
            progressManager->setProgress(
                m_activityId, WorkbenchProgressManager::kFailedProgressValue);
            progressManager->setTitle(m_activityId, tr("File downloading failed"));
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

void WebDownloader::onStateChanged()
{
    int state = m_item->property("state").toInt();
    switch (state)
    {
        case QWebEngineDownloadItem::DownloadRequested:
        case QWebEngineDownloadItem::DownloadInProgress:
            setState(State::Downloading);
            break;
        case QWebEngineDownloadItem::DownloadCompleted:
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
    auto progressManager = context()->instance<WorkbenchProgressManager>();
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

    progressManager->setProgress(m_activityId, static_cast<qreal>(bytesRead) / bytesTotal);
    progressManager->setProgressFormat(m_activityId, QString("%1, %p%").arg(speed));
}
