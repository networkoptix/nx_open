#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

#include "wearable_payload.h"

class QnTimePeriodList;

namespace nx {
namespace client {
namespace desktop {

class WearablePreparer:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    WearablePreparer(const QnSecurityCamResourcePtr& camera, QObject* parent = nullptr);
    virtual ~WearablePreparer() override;

    void prepareUploads(const QStringList& filePaths, const QnTimePeriodList& queue);

private:
    void checkLocally(WearablePayload& payload);
    void handlePrepareFinished(bool success, const QnWearableCheckReply& reply);

signals:
    void finished(const WearableUpload& result);
    void finishedLater(const WearableUpload& result);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx

