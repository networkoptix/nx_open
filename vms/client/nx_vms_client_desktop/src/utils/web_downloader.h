// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QFileInfo>
#include <QtCore/QPointer>
#include <QtWebEngineCore/QWebEngineDownloadRequest>

#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {
namespace utils {

/**
 * Enables saving and downloading QQuickWebEngineDownloadItem
 * and reports download progress in right panel.
 */
class WebDownloader: public QObject, public WindowContextAware
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

    WebDownloader(
        WindowContext* windowContext,
        QObject* parent,
        const QString& filePath,
        QObject* item);

    virtual ~WebDownloader();

public:
    // Handle download from QWebEngine-based browser.
    static bool download(QObject* item, WindowContext* context);

    static QString selectFile(const QString& suggestedName, QWidget* widget);

private:
    void startDownload();
    void setState(State state);

private slots:
    void cancel();
    void onDownloadProgress(qint64 bytesRead, qint64 bytesTotal);

    void onReceivedBytesChanged();
    void onStateChanged(QWebEngineDownloadRequest::DownloadState state);

private:
    State m_state = State::Init;
    QElapsedTimer m_downloadTimer;
    QFileInfo m_fileInfo;
    QnUuid m_notificationId;
    bool m_cancelRequested = false;
    bool m_hasWriteError = false;
    QPointer<QObject> m_item;
    qint64 m_initBytesRead = 0;
    qint64 m_initElapsed = 0;
};

} // namespace utils
} // namespace nx::vms::client::desktop
