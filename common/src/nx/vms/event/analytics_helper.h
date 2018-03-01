#pragma once

#include <QtCore/QObject>

#include <api/model/analytics_actions.h>
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

    EventDescriptor eventDescriptor(const QnUuid& eventId) const;

    /** Get list of all supported analytics events for the given cameras. */
    static QList<EventDescriptor> supportedAnalyticsEvents(
        const QnVirtualCameraResourceList& cameras);

    static bool hasDifferentDrivers(const QList<EventDescriptor>& events);

    static QString eventName(const QnVirtualCameraResourcePtr& camera,
        const QnUuid& eventTypeId,
        const QString& locale);

    static QString objectName(const QnVirtualCameraResourcePtr& camera,
        const QnUuid& objectTypeId,
        const QString& locale);

    struct PluginActions
    {
        QnUuid driverId;
        QList<api::AnalyticsManifestObjectAction> actions;
    };

    static QList<PluginActions> availableActions(
        const QnMediaServerResourceList& servers, const QnUuid& objectTypeId);
};

} // namespace event
} // namespace vms
} // namespace nx
