#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

#include "wearable_state.h"
#include "wearable_error.h"

struct QnWearableStatusReply;

namespace nx {
namespace client {
namespace desktop {

struct UploadState;

class WearableWorker:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    WearableWorker(
        const QnSecurityCamResourcePtr& camera,
        const QnUserResourcePtr& user,
        QObject* parent = nullptr);
    virtual ~WearableWorker() override;

    void updateState();
    bool addUpload(const QString& path, WearableError* error);

    WearableState state() const;

    bool isWorking() const;

signals:
    void stateChanged(const WearableState& state);
    void error(const WearableState& state, const QString& errorMessage);

private:
    void processCurrentFile();
    void processNextFile();

    void handleStatusFinished(bool success, const QnWearableStatusReply& result);
    void handleLockFinished(bool success, const QnWearableStatusReply& result);
    void handleExtendFinished(bool success, const QnWearableStatusReply& result);
    void handleUnlockFinished(bool success, const QnWearableStatusReply& result);
    void handleUploadProgress(const UploadState& state);
    void handleConsumeStarted(bool success);

    void pollExtend();

private:
    QString calculateUserName(const QnUuid& userId);
    QString calculateLockedMessage();

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx

