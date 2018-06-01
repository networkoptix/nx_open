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
    struct EventDescriptor: public nx::api::Analytics::EventType
    {
        using base_type = nx::api::Analytics::EventType;

        EventDescriptor() {}
        EventDescriptor(const base_type& value): base_type(value) {}

        QnUuid driverId;
        nx::api::TranslatableString driverName;
    };

    AnalyticsHelper(QnCommonModule* commonModule, QObject* parent = nullptr);

    /** Get list of all supported analytics events in the system. */
    QList<EventDescriptor> systemSupportedAnalyticsEvents() const;

    /** Get list of all camera-independent analytics events in the system. */
    QList<EventDescriptor> systemCameraIndependentAnalyticsEvents() const;

    EventDescriptor eventDescriptor(const QnUuid& eventId) const;
    nx::api::Analytics::Group groupDescriptor(const QnUuid& groupId) const;

    /** Get list of all supported analytics events for the given cameras. */
    static QList<EventDescriptor> supportedAnalyticsEvents(
        const QnVirtualCameraResourceList& cameras);

    /** Get list of all camera-independent analytics events from given servers. */
    static QList<EventDescriptor> cameraIndependentAnalyticsEvents(
        const QnMediaServerResourceList& servers);

    static bool hasDifferentDrivers(const QList<EventDescriptor>& events);

    static QString eventName(const QnVirtualCameraResourcePtr& camera,
        const QnUuid& eventTypeId,
        const QString& locale);

};

} // namespace event
} // namespace vms
} // namespace nx
