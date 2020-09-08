#pragma once

#include <nx/vms/server/analytics/wrappers/sdk_object_description.h>
#include <nx/vms/server/analytics/wrappers/types.h>

#include <nx/vms/server/sdk_support/error.h>

namespace nx::vms::server::analytics::wrappers {

class PluginDiagnosticMessageBuilder
{
public:
    PluginDiagnosticMessageBuilder(
        SdkMethod sdkMethod,
        SdkObjectDescription sdkObjectDescription,
        sdk_support::Error error);

    PluginDiagnosticMessageBuilder(
        SdkMethod sdkMethod,
        SdkObjectDescription sdkObjectDescription,
        Violation violation);

    PluginDiagnosticMessageBuilder(
        SdkObjectDescription sdkObjectDescription,
        QString suspicionCaption,
        QString suspicionDetails);

    QString buildLogString() const;
    QString buildTechnicalDetails() const;
    QString buildPluginDiagnosticEventCaption() const;
    QString buildPluginDiagnosticEventDescription() const;

private:
    const SdkMethod m_sdkMethod = SdkMethod::undefined;
    const SdkObjectDescription m_sdkObjectDescription;
    const sdk_support::Error m_error;
    const Violation m_violation;
    const QString m_suspicionCaption;
    const QString m_suspicionDetails;
};

} // namespace nx::vms::server::analytics::wrappers
