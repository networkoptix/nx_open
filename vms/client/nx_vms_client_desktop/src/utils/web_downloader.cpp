#include "web_downloader.h"
#include "reply_read_timeout.h"

#include <QDir>
#include <QAction>
#include <QFile>
#include <QTimer>
#include <QWebEngineDownloadItem>

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

static const qint64 kDownloadBufferSize = 64 * 1024;
static const qint64 kQuietStopTimeoutMs = 10000;
static const auto kDataReadTimeout = std::chrono::milliseconds(60000);

QString getSuggestedFileName(const QNetworkReply* reply)
{
    const QString urlStr = reply->url().toString();
    QString nameFromUrl = urlStr.mid(urlStr.lastIndexOf('/') + 1);
    int paramsIndex = nameFromUrl.indexOf('?');
    if (paramsIndex != -1)
        nameFromUrl = nameFromUrl.left(paramsIndex);

    if (!reply->rawHeaderList().contains("Content-Disposition"))
        return nameFromUrl;

    static const QRegularExpression re(
        "filename\\*?=['\"]?(?:UTF-\\d['\"]*)?([^;\\r\\n\"']*)['\"]?;?");

    const QRegularExpressionMatch match = re.match(reply->rawHeader("Content-Disposition"));
    if (!match.hasMatch())
        return nameFromUrl;

    return match.captured(1);
}

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

WebDownloader::WebDownloader(QObject* parent,
    std::shared_ptr<QNetworkAccessManager> networkManager,
    QNetworkReply* reply,
    std::unique_ptr<QFile> file,
    const QFileInfo& fileInfo):
    base_type(parent),
    QnWorkbenchContextAware(parent), m_networkManager(networkManager), m_reply(reply),
    m_file(std::move(file)), m_fileInfo(fileInfo), m_item(nullptr)
{
    m_reply->setParent(this);
    startDownload();
}

WebDownloader::WebDownloader(QObject* parent,
    const QFileInfo& fileInfo,
    QObject* item):
    base_type(parent),
    QnWorkbenchContextAware(parent), m_networkManager(nullptr), m_reply(nullptr),
    m_file(nullptr), m_fileInfo(fileInfo), m_item(item)
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

    if (!m_file)
        return;

    // Cleanup incomplete files.
    if (m_file->isOpen())
        m_file->close();

    m_file->remove();
}

bool WebDownloader::download(QObject* item, QnWorkbenchContext* context)
{
    const auto suggestedName = QFileInfo(item->property("path").toString()).fileName();

    std::unique_ptr<QFile> file;
    QFileInfo fileInfo;

    ContextHelper contextHelper(context);

    if (!selectFile(suggestedName, contextHelper.windowWidget(), file, fileInfo))
        return false;

    file->close();
    file->remove();

    item->setProperty("path", fileInfo.absoluteFilePath());

    new WebDownloader(context->instance<WorkbenchProgressManager>(), fileInfo, item);

    return true;
}

bool WebDownloader::download(
    std::shared_ptr<QNetworkAccessManager> manager, QNetworkReply* reply, QObject* parent)
{
    // Avoid blocking event loop for writing long chunks.
    reply->setReadBufferSize(kDownloadBufferSize);

    const auto suggestedName = getSuggestedFileName(reply);

    std::unique_ptr<QFile> file;
    QFileInfo fileInfo;

    ContextHelper contextHelper(parent);

    if (!selectFile(suggestedName, contextHelper.windowWidget(), file, fileInfo))
        return false;

    new WebDownloader(contextHelper.context()->instance<WorkbenchProgressManager>(), manager, reply, std::move(file), fileInfo);

    return true;
}

bool WebDownloader::selectFile(const QString& suggestedName, QWidget* widget, std::unique_ptr<QFile>& file, QFileInfo& fileInfo)
{
    auto lastDir = qnSettings->lastDownloadDir();
    if (lastDir.isEmpty() || !QDir(lastDir).exists())
        lastDir = qnSettings->mediaFolder();

    auto filePath = getUniqueFilePath(QDir(lastDir).filePath(suggestedName));

    // Loop until we have a writable file path.
    while (true)
    {
        QnCustomFileDialog saveDialog(widget,
            tr("Save File As..."),
            filePath,
            QnCustomFileDialog::createFilter(QnCustomFileDialog::kAllFilesFilter));
        saveDialog.setFileMode(QFileDialog::AnyFile);
        saveDialog.setAcceptMode(QFileDialog::AcceptSave);

        if (!saveDialog.exec())
            return false;

        filePath = saveDialog.selectedFile();

        if (filePath.isEmpty())
            return false;

        fileInfo = QFileInfo(filePath);
        file = std::make_unique<QFile>();
        file->setFileName(fileInfo.absoluteFilePath());
        if (file->open(QIODevice::WriteOnly))
            break; // Break from the loop if we got a writable file.

        QnMessageBox messageBox(widget);
        messageBox.setIcon(QnMessageBoxIcon::Critical);
        if (fileInfo.exists())
        {
            messageBox.setText(tr("Failed to overwrite file"));
            messageBox.setInformativeText(QDir::toNativeSeparators(fileInfo.absoluteFilePath()));
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

    qnSettings->setLastDownloadDir(fileInfo.absolutePath());

    return true;
}

void WebDownloader::startDownload()
{
    auto progressManager = context()->instance<WorkbenchProgressManager>();
    m_activityId = progressManager->add(
        tr("Downloading file..."), m_fileInfo.fileName(), /*cancellable*/ true);
    setState(State::Downloading);

    auto action = CommandActionPtr(new CommandAction(tr("Open Containing Folder")));
    connect(action.data(), &QAction::triggered, this, [this]() {
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

    if (m_item)
    {
        connect(m_item, SIGNAL(receivedBytesChanged()), this, SLOT(onReceivedBytesChanged()));
        connect(m_item, SIGNAL(stateChanged()), this, SLOT(onStateChanged()));
        DownloadItemReadTimeout::set(m_item, kDataReadTimeout);
        QMetaObject::invokeMethod(m_item, "accept");
        return;
    }

    if (!m_reply)
        return;

    connect(m_reply, &QNetworkReply::readyRead, this, &WebDownloader::writeAvailableData);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &WebDownloader::onDownloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, &WebDownloader::onReplyFinished);

    ReplyReadTimeout::set(m_reply, kDataReadTimeout);

    // If we missed readyRead or finished signals while save dialog event loop was running...
    if (m_reply->isRunning())
        writeAvailableData();

    if (m_reply->isFinished())
    {
        if (m_reply->error() == QNetworkReply::NoError)
            writeAvailableData();
        onReplyFinished();
    }
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

    if (m_file)
    {
        m_file->remove();
        m_reply->abort();
    }

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

void WebDownloader::writeAvailableData()
{
    const auto data = m_reply->readAll();
    if (m_file->write(data) != data.size())
    {
        m_hasWriteError = true;
        m_reply->abort();
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

void WebDownloader::onReplyFinished()
{
    const bool success = m_reply->error() == QNetworkReply::NoError && !m_hasWriteError;

    if (success)
    {
        setState(State::Completed);
    }
    else
    {
        // Show failed notification only if there was no cancel request from the user.
        // If the user manually canceled the downloading process then silently remove the informer.
        if (!m_cancelRequested)
            setState(State::Failed);
    }

    if (m_file->isOpen())
        m_file->close();

    if (!success)
        m_file->remove();
}
