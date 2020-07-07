#include "settings_engine_wrapper.h"

#include <utils/common/synctime.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/server/resource/camera.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/analytics/wrappers/sdk_object_description.h>

namespace nx::vms::server::analytics {

using Issue = nx::vms::server::interactive_settings::Issue;

static QString buildIssuesDescription(const QList<Issue>& issues, Issue::Type issueType)
{
    QString result;

    for (const Issue& issue: issues)
    {
        if (issue.type != issueType)
            continue;

        // Concatenate all issues via a space.
        result += NX_FMT("[%1] %2 ", issue.code, issue.message);
    }

    if (!result.isEmpty())
        result = result.left(result.size() - 2);

    return result;
}

SettingsEngineWrapper::SettingsEngineWrapper(
    event::EventConnector* eventConnector,
    resource::AnalyticsEngineResourcePtr engine,
    resource::CameraPtr device)
    :
    m_device(std::move(device)),
    m_engine(std::move(engine))
{
    NX_ASSERT(m_engine);
    connect(this, &SettingsEngineWrapper::pluginDiagnosticEventTriggered,
        eventConnector, &event::EventConnector::at_pluginDiagnosticEvent,
        Qt::QueuedConnection);
}

bool SettingsEngineWrapper::loadModelFromJsonObject(const QJsonObject& json)
{
    const bool result = m_settingsEngine.loadModelFromJsonObject(json);
    reportIssues("load Settings Model", m_settingsEngine.issues());
    return result;
}

void SettingsEngineWrapper::applyValues(const QJsonObject& values)
{
    m_settingsEngine.applyValues(values);
    reportIssues("apply Settings", m_settingsEngine.issues());
}

void SettingsEngineWrapper::applyStringValues(const QJsonObject& values)
{
    m_settingsEngine.applyStringValues(values);
    reportIssues("apply Settings from the Plugin", m_settingsEngine.issues());
}

QJsonObject SettingsEngineWrapper::values() const
{
    return m_settingsEngine.values();
}

QJsonObject SettingsEngineWrapper::serializeModel() const
{
    return m_settingsEngine.serializeModel();
}

void SettingsEngineWrapper::reportIssues(
    const QString& operationDescription,
    const QList<nx::vms::server::interactive_settings::Issue>& issues)
{
    const QString issueSource = wrappers::SdkObjectDescription(
        m_engine->plugin().dynamicCast<nx::vms::server::resource::AnalyticsPluginResource>(),
        m_engine,
        m_device
    ).descriptionString();

    const QString caption = "Issue(s) with Analytics Plugin settings detected";
    const QString descriptionPrefix = lm("Server cannot %1. Technical details: %2: ").args(
        operationDescription, issueSource);
    const QString warningDescription = buildIssuesDescription(issues, Issue::Type::warning);
    const QString errorDescription = buildIssuesDescription(issues, Issue::Type::error);

    const qint64 currentTimeUs = qnSyncTime->currentUSecsSinceEpoch();

    if (!warningDescription.isEmpty())
    {
        const nx::vms::event::PluginDiagnosticEventPtr pluginDiagnosticEvent(
            new nx::vms::event::PluginDiagnosticEvent(
                currentTimeUs,
                m_engine ? m_engine->getId() : QnUuid(),
                caption,
                descriptionPrefix + warningDescription,
                nx::vms::api::EventLevel::WarningEventLevel,
                m_device));

        pluginDiagnosticEventTriggered(pluginDiagnosticEvent);
    }

    if (!errorDescription.isEmpty())
    {
        const nx::vms::event::PluginDiagnosticEventPtr pluginDiagnosticEvent(
            new nx::vms::event::PluginDiagnosticEvent(
                currentTimeUs,
                m_engine ? m_engine->getId() : QnUuid(),
                caption,
                descriptionPrefix + errorDescription,
                nx::vms::api::EventLevel::ErrorEventLevel,
                m_device));

        pluginDiagnosticEventTriggered(pluginDiagnosticEvent);
    }
}

} // namespace nx::vms::server::analytics
