#pragma once

#include <QtCore/QMetaObject>

#include <utils/common/synctime.h>

#include <nx/sdk/helpers/ptr.h>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/plugin_diagnostic_event.h>

#include <nx/vms/server/analytics/wrappers/types.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/sdk_support/error.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/server/analytics/wrappers/string_builder.h>
#include <nx/vms/server/analytics/wrappers/manifest_processor.h>

namespace nx::vms::server::analytics::wrappers {

template<typename MainSdkObject, typename ManifestType>
class BasePlugin: public /*mixin*/ ServerModuleAware
{

public:
    BasePlugin(
        QnMediaServerModule* serverModule,
        sdk::Ptr<MainSdkObject> sdkObject,
        QString libName)
        :
        ServerModuleAware(serverModule),
        m_mainSdkObject(sdkObject),
        m_libName(libName)
    {
        if (!NX_ASSERT(m_mainSdkObject))
            return;
    }

    std::optional<ManifestType> manifest() const
    {
        if (!NX_ASSERT(m_mainSdkObject))
            return std::nullopt;

        ManifestProcessor<ManifestType> manifestProcessor(
            makeProcessorSettings(),
            sdkObjectDescription(),
            [this](Violation violation)
            {
                handleViolation(SdkMethod::manifest, violation, /*returnValue*/ nullptr);
            },
            [this](sdk_support::Error error)
            {
                handleError(SdkMethod::manifest, error, /*returnValue*/ nullptr);
            });

        return manifestProcessor.manifest(m_mainSdkObject);
    }

    bool isValid() const { return sdkObject(); }

    sdk::Ptr<MainSdkObject> sdkObject() const { return m_mainSdkObject; };

    QString libName() const { return m_libName; }

protected:
    virtual DebugSettings makeProcessorSettings() const = 0;

    virtual SdkObjectDescription sdkObjectDescription() const = 0;

protected:
    template<typename GenericError, typename ReturnType>
    ReturnType handleGenericError(
        SdkMethod sdkMethod,
        GenericError error,
        ReturnType returnValue) const
    {
        StringBuilder stringBuilder(sdkMethod, sdkObjectDescription(), error);
        NX_DEBUG(this, stringBuilder.buildLogString());
        throwPluginEvent(
            stringBuilder.buildPluginDiagnosticEventCaption(),
            stringBuilder.buildPluginDiagnosticEventDescription());

        return returnValue;
    }

    template<typename ReturnType>
    ReturnType handleError(
        SdkMethod sdkMethod,
        const sdk_support::Error& error,
        ReturnType returnValue) const
    {
        if (!NX_ASSERT(!error.isOk()))
            return returnValue;

        return handleGenericError(sdkMethod, error, returnValue);
    }

    template<typename ReturnType>
    ReturnType handleViolation(
        SdkMethod sdkMethod,
        const Violation violation,
        ReturnType returnValue) const
    {
        return handleGenericError(sdkMethod, violation, returnValue);
    }

    void throwPluginEvent(
        const QString& caption,
        const QString& description) const
    {
        using namespace nx::vms::event;
        const auto sdkObjectDescription = this->sdkObjectDescription();

        const auto engineResource = sdkObjectDescription.engine();
        const QnUuid engineId = engineResource ? engineResource->getId() : QnUuid();

        PluginDiagnosticEventPtr pluginDiagnosticEvent(
            new PluginDiagnosticEvent(
                qnSyncTime->currentUSecsSinceEpoch(),
                engineId,
                caption,
                description,
                nx::vms::api::EventLevel::ErrorEventLevel,
                sdkObjectDescription.device()));

        // TODO: better introduce a helper class and use a real connection instead of
        // `invokeMethod`.
        QMetaObject::invokeMethod(
            serverModule()->eventConnector(),
            "at_pluginDiagnosticEvent",
            Qt::QueuedConnection,
            Q_ARG(nx::vms::event::PluginDiagnosticEventPtr, pluginDiagnosticEvent));
    }

private:
    sdk::Ptr<MainSdkObject> m_mainSdkObject;
    QString m_libName;
};

} // namespace nx::vms::server::analytics::wrappers
