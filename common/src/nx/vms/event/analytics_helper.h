#pragma once

#include <QtCore/QObject>

#include <api/model/analytics_actions.h>
#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms/api/analytics/plugin_manifest.h>

namespace nx {
namespace vms {
namespace event {

class AnalyticsHelper: public QObject, public QnCommonModuleAware
{
    using base_type = QObject;
    Q_OBJECT
public:
    struct EventTypeDescriptor: public nx::vms::api::analytics::EventType
    {
        using base_type = nx::vms::api::analytics::EventType;

        EventTypeDescriptor() {}
        EventTypeDescriptor(const base_type& value): base_type(value) {}

        QString pluginId;
        nx::vms::api::analytics::TranslatableString pluginName;
    };

    AnalyticsHelper(QnCommonModule* commonModule, QObject* parent = nullptr);

    /** Get list of all supported analytics events in the system. */
    QList<EventTypeDescriptor> systemSupportedAnalyticsEvents() const;

    /** Get list of all camera-independent analytics events in the system. */
    QList<EventTypeDescriptor> systemCameraIndependentAnalyticsEvents() const;

    EventTypeDescriptor eventTypeDescriptor(const QString& eventTypeId) const;
    nx::vms::api::analytics::Group groupDescriptor(const QString& groupId) const;

    /** Get list of all supported analytics events for the given cameras. */
    static QList<EventTypeDescriptor> supportedAnalyticsEvents(
        const QnVirtualCameraResourceList& cameras);

    /** Get list of all camera-independent analytics events from given servers. */
    static QList<EventTypeDescriptor> cameraIndependentAnalyticsEvents(
        const QnMediaServerResourceList& servers);

    static bool hasDifferentDrivers(const QList<EventTypeDescriptor>& events);

    static QString eventTypeName(
        const QnVirtualCameraResourcePtr& camera,
        const QString& eventTypeId,
        const QString& locale);

    static QString objectTypeName(
        const QnVirtualCameraResourcePtr& camera,
        const QString& objectTypeId,
        const QString& locale);

    struct PluginActions
    {
        QString pluginId;
        QList<nx::vms::api::analytics::PluginManifest::ObjectAction> actions;
    };

    static QList<PluginActions> availableActions(
        const QnMediaServerResourceList& servers, const QString& objectTypeId);
};

} // namespace event
} // namespace vms
} // namespace nx
