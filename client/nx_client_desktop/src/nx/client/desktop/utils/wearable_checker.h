#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

#include "wearable_payload.h"

namespace nx {
namespace client {
namespace desktop {

class WearableChecker:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    WearableChecker(const QnSecurityCamResourcePtr& camera, QObject* parent = nullptr);
    virtual ~WearableChecker() override;

    void checkUploads(const QStringList& filePaths);

private:
    void checkLocally(WearablePayload& payload);
    void handleCheckFinished(bool success, const QnWearableCheckReply& reply);

signals:
    void finished(const WearablePayloadList& result);
    void finishedLater(const WearablePayloadList& result);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx

