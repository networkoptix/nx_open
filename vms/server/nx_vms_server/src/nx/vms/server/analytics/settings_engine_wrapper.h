#pragma once

#include <utils/common/connective.h>
#include <nx/vms/server/interactive_settings/json_engine.h>
#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/event/events/plugin_diagnostic_event.h>

namespace nx::vms::server::event { class EventConnector; }

namespace nx::vms::server::analytics {

class SettingsEngineWrapper: public Connective<QObject>
{
    Q_OBJECT

public:
    SettingsEngineWrapper(
        event::EventConnector* eventConnector,
        resource::AnalyticsEngineResourcePtr engine,
        resource::CameraPtr device = resource::CameraPtr());

    bool loadModelFromJsonObject(const QJsonObject& json);
    void applyValues(const QJsonObject& values);
    void applyStringValues(const QJsonObject& values);

    QJsonObject values() const;
    QJsonObject serializeModel() const;

signals:
    void pluginDiagnosticEventTriggered(const nx::vms::event::PluginDiagnosticEventPtr&) const;

private:
    void reportIssues(
        const QString& operationDescription,
        const QList<nx::vms::server::interactive_settings::Issue>& issues);

private:
    interactive_settings::JsonEngine m_settingsEngine;

    nx::vms::server::resource::CameraPtr m_device;
    nx::vms::server::resource::AnalyticsEngineResourcePtr m_engine;
};

} // namespace nx::vms::server::analytics
