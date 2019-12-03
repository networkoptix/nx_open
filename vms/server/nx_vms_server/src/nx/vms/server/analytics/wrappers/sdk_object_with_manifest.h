#pragma once

#include <QtCore/QMetaObject>

#include <utils/common/synctime.h>
#include <plugins/vms_server_plugins_ini.h>

#include <nx/sdk/ptr.h>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/plugin_diagnostic_event.h>

#include <nx/vms/server/analytics/wrappers/types.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/sdk_support/error.h>
#include <nx/vms/server/sdk_support/timed_guard.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/server/analytics/wrappers/string_builder.h>
#include <nx/vms/server/analytics/wrappers/manifest_processor.h>
#include <nx/vms/server/analytics/wrappers/method_timeouts.h>

namespace nx::vms::server::analytics::wrappers {

template<typename MainSdkObject, typename ManifestType>
class SdkObjectWithManifest: public /*mixin*/ ServerModuleAware
{
public:
    SdkObjectWithManifest(
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

    virtual ~SdkObjectWithManifest() = default;

    std::optional<ManifestType> manifest(
        std::unique_ptr<StringBuilder>* outStringBuilder = nullptr) const
    {
        sdk_support::TimedGuard guard = makeTimedGuard(SdkMethod::manifest);

        if (!NX_ASSERT(m_mainSdkObject))
            return std::nullopt;

        ManifestProcessor<ManifestType> manifestProcessor(
            makeManifestProcessorSettings(),
            sdkObjectDescription(),
            [this, outStringBuilder](Violation violation)
            {
                if (outStringBuilder)
                {
                    *outStringBuilder =
                        std::make_unique<StringBuilder>(
                            SdkMethod::manifest, sdkObjectDescription(), violation);
                }

                handleViolation(SdkMethod::manifest, violation, /*returnValue*/ nullptr);
            },
            [this, outStringBuilder](sdk_support::Error error)
            {
                if (outStringBuilder)
                {
                    *outStringBuilder =
                        std::make_unique<StringBuilder>(
                            SdkMethod::manifest, sdkObjectDescription(), error);
                }

                handleError(SdkMethod::manifest, error, /*returnValue*/ nullptr);
            });

        return manifestProcessor.manifest(m_mainSdkObject);
    }

    bool isValid() const { return sdkObject(); }

    sdk::Ptr<MainSdkObject> sdkObject() const { return m_mainSdkObject; };

    QString libName() const { return m_libName; }

protected:
    virtual DebugSettings makeManifestProcessorSettings() const = 0;

    virtual SdkObjectDescription sdkObjectDescription() const = 0;

protected:
    template<typename GenericError, typename ReturnType>
    ReturnType handleGenericError(
        SdkMethod sdkMethod,
        const GenericError& error,
        ReturnType returnValue,
        bool shouldTriggerAssert = false) const
    {
        const StringBuilder stringBuilder(sdkMethod, sdkObjectDescription(), error);

        if (shouldTriggerAssert)
            NX_ASSERT(false, stringBuilder.buildLogString());
        else
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
        ReturnType returnValue,
        bool shouldTriggerAssert = false) const
    {
        if (!NX_ASSERT(!error.isOk()))
            return returnValue;

        return handleGenericError(sdkMethod, error, returnValue, shouldTriggerAssert);
    }

    template<typename ReturnType>
    ReturnType handleViolation(
        SdkMethod sdkMethod,
        const Violation& violation,
        ReturnType returnValue,
        bool shouldTriggerAssert = false) const
    {
        return handleGenericError(sdkMethod, violation, returnValue, shouldTriggerAssert);
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

    sdk_support::TimedGuard makeTimedGuard(
        SdkMethod sdkMethod,
        QString additionalInfo = QString()) const
    {
        return sdk_support::TimedGuard(
            sdkMethodTimeout(sdkMethod),
            [this, sdkMethod, additionalInfo = std::move(additionalInfo)]()
            {
                handleViolation(
                    sdkMethod,
                    {ViolationType::methodExecutionTookTooLong, std::move(additionalInfo)},
                    /*returnValue*/ nullptr,
                    pluginsIni().shouldMethodTimeoutViolationTriggerAnAssertion);
            });
    }

private:
    sdk::Ptr<MainSdkObject> m_mainSdkObject;
    QString m_libName;
};

} // namespace nx::vms::server::analytics::wrappers
