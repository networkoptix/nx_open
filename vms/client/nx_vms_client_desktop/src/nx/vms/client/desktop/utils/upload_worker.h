#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>

#include <nx/vms/common/p2p/downloader/result_code.h>
#include "upload_state.h"

namespace nx::vms::client::desktop {

class UploadWorker: public QObject
{
    Q_OBJECT

public:
    UploadWorker(
        const QnMediaServerResourcePtr& server,
        UploadState& config,
        QObject* parent = nullptr);
    virtual ~UploadWorker() override;

    void start();
    void cancel();

    UploadState state() const;

    using RemoteResult = nx::vms::common::p2p::downloader::ResultCode;

signals:
    void progress(const UploadState&);

private:
    // TODO: Move to PIMPL.
    void emitProgress();
    void handleStop();
    void handleError(const QString& message);
    void handleMd5Calculated();

    void handleUpload();
    void handleChunkUploaded(bool success);
    void handleAllUploaded();
    void handleCheckFinished(bool success, bool ok);
    void handleFileUploadCreated(bool success, RemoteResult code, QString error);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop

