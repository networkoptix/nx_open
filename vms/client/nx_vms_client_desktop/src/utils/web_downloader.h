#pragma once

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QElapsedTimer>
#include <QtNetwork/QNetworkReply>

#include <nx/utils/uuid.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {
namespace utils {

/**
 * Downloads QNetworkReply or QQuickWebEngineDownloadItem and reports download progress in right panel.
 */
class WebDownloader: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

    enum class State
    {
        Init,
        Downloading,
        Completed,
        Failed
    };

    WebDownloader(QObject* parent,
        std::shared_ptr<QNetworkAccessManager> networkManager,
        QNetworkReply* reply,
        std::unique_ptr<QFile> file,
        const QFileInfo& fileInfo);

    WebDownloader(QObject* parent,
        const QFileInfo& fileInfo,
        QObject* item);

    virtual ~WebDownloader();

public:

    // Handle download from QWebKit-based browser.
    static bool download(
        std::shared_ptr<QNetworkAccessManager> manager, QNetworkReply* reply, QObject* parent);

    // Handle download from QWebEngine-based browser.
    static bool download(QObject* item, QnWorkbenchContext* context);

private:
    void startDownload();
    void setState(State state);
    void removeProgress();
    static bool selectFile(const QString& suggestedName,
        QWidget* widget,
        std::unique_ptr<QFile>& file,
        QFileInfo& fileInfo);

private slots:
    void cancel();
    void onDownloadProgress(qint64 bytesRead, qint64 bytesTotal);
    void writeAvailableData();
    void onReplyFinished();

    void onReceivedBytesChanged();
    void onStateChanged();

private:
    State m_state = State::Init;
    QElapsedTimer m_downloadTimer;
    std::shared_ptr<QNetworkAccessManager> m_networkManager;
    QNetworkReply* m_reply;
    std::unique_ptr<QFile> m_file;
    QFileInfo m_fileInfo;
    QnUuid m_activityId;
    bool m_cancelRequested = false;
    bool m_hasWriteError = false;
    QPointer<QObject> m_item;
    qint64 m_initBytesRead = 0;
    qint64 m_initElapsed = 0;
};

} // namespace utils
} // namespace nx::vms::client::desktop
