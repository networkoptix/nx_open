#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

#include "wearable_fwd.h"

struct QnWearablePrepareReply;
class QnTimePeriodList;

namespace nx::vms::client::desktop {

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
    void handlePrepareFinished(bool success, const QnWearablePrepareReply& reply);

signals:
    void finished(const WearableUpload& result);
    void finishedLater(const WearableUpload& result);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop

