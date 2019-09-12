#pragma once

#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QtNetwork/QNetworkReply>

#include <nx/utils/uuid.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {
namespace utils {

/*
 * Downloads QNetworkReply and reports download progress in right panel.
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

public:
    WebDownloader(QObject* parent,
        std::shared_ptr<QNetworkAccessManager> networkManager,
        QNetworkReply* reply,
        QFile* file,
        const QFileInfo& fileInfo);
    virtual ~WebDownloader();

    static bool download(
        std::shared_ptr<QNetworkAccessManager> manager, QNetworkReply* reply, QObject* parent);

private:
    void startDownload();
    void setState(State state);
    void removeProgress();

private slots:
    void cancel();
    void onDownloadProgress(qint64 bytesRead, qint64 bytesTotal);
    void onReadyRead();
    void onReplyFinished();

private:
    State m_state = State::Init;
    QElapsedTimer m_downloadTimer;
    std::shared_ptr<QNetworkAccessManager> m_networkManager;
    QNetworkReply* m_reply;
    QFile* m_file;
    QFileInfo m_fileInfo;
    QnUuid m_activityId;
    bool m_cancelRequested = false;
};

} // namespace utils
} // namespace nx::vms::client::desktop
