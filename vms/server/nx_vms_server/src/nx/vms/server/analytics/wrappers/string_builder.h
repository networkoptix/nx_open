#pragma once

#include <nx/vms/server/analytics/wrappers/sdk_object_description.h>
#include <nx/vms/server/analytics/wrappers/types.h>

#include <nx/vms/server/sdk_support/error.h>

namespace nx::vms::server::analytics::wrappers {

class StringBuilder
{
public:
    StringBuilder(
        SdkMethod sdkMethod,
        SdkObjectDescription sdkObjectDescription,
        sdk_support::Error error);

    StringBuilder(
        SdkMethod sdkMethod,
        SdkObjectDescription sdkObjectDescription,
        Violation violation);

    QString buildLogString() const;

    QString buildPluginDiagnosticEventCaption() const;

    QString buildPluginDiagnosticEventDescription() const;

private:
    QString buildViolationShortString() const;
    QString buildViolationFullString() const;

    QString buildErrorShortString() const;
    QString buildErrorFullString() const;

private:
    const SdkMethod m_sdkMethod = SdkMethod::undefined;
    const SdkObjectDescription m_sdkObjectDescription;
    const sdk_support::Error m_error;
    const Violation m_violation;

};

} // namespace nx::vms::server::analytics::wrappers
