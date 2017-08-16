#pragma once

#include <QtCore/QObject>

#include <common/common_module_aware.h>

#include <core/resource/resource_fwd.h>

#include <nx/api/analytics/analytics_event.h>
#include <nx/api/analytics/driver_manifest.h>

namespace nx {
namespace vms {
namespace event {

class AnalyticsHelper: public QObject, public QnCommonModuleAware
{
    using base_type = QObject;
    Q_OBJECT
public:
    AnalyticsHelper(QnCommonModule* commonModule, QObject* parent = nullptr);

    QList<nx::api::AnalyticsEventTypeWithRef> analyticsEvents() const;

    static bool hasDifferentDrivers(const QList<nx::api::AnalyticsEventTypeWithRef>& events);

    static QString eventName(const QnVirtualCameraResourcePtr& camera,
        const QnUuid& eventTypeId,
        const QString& locale);

};

} // namespace event
} // namespace vms
} // namespace nx
