#include "string_builder.h"

#include <nx/vms/server/sdk_support/to_string.h>
#include <nx/fusion/model_functions.h>

namespace nx::vms::server::analytics::wrappers {

static QString toHumanReadableString(SdkObjectType sdkObjectType)
{
    return QnLexical::serialized(sdkObjectType);
}

static QString toHumanReadableString(SdkMethod sdkMethod)
{
    return QnLexical::serialized(sdkMethod) + "()";
}

static QString toHumanReadableString(ViolationType violation)
{
    switch (violation)
    {
        case ViolationType::internalViolation:
            return "internal violation";
        case ViolationType::nullManifest:
            return "Manifest is null";
        case ViolationType::nullManifestString:
            return "Manifest string is null";
        case ViolationType::emptyManifestString:
            return "Manifest string is empty";
        case ViolationType::invalidJson:
            return "Manifest deserialization error - manifest is not a valid JSON";
        case ViolationType::invalidJsonStructure:
        {
            return "Manifest deserialization error - unable to deserialize provided JSON to the "
                "Manifest structure";
        }
        case ViolationType::manifestLogicalError:
            return "Manifest contains logical errors";
        case ViolationType::nullEngine:
            return "Engine is null while no error present";
        case ViolationType::nullDeviceAgent:
            return "DeviceAgent is null while no error present";
        default:
            NX_ASSERT(false);
            return QString("unknown violation");
    }
}

StringBuilder::StringBuilder(
    SdkMethod sdkMethod,
    SdkObjectDescription sdkObjectDescription,
    sdk_support::Error error)
    :
    m_sdkMethod(sdkMethod),
    m_sdkObjectDescription(std::move(sdkObjectDescription)),
    m_error(std::move(error))
{
}

StringBuilder::StringBuilder(
    SdkMethod sdkMethod,
    SdkObjectDescription sdkObjectDescription,
    Violation violation)
    :
    m_sdkMethod(sdkMethod),
    m_sdkObjectDescription(std::move(sdkObjectDescription)),
    m_violation(violation)
{
}

QString StringBuilder::buildLogString() const
{
    if (m_violation.type != ViolationType::undefined)
        return buildViolationFullString();

    return buildErrorFullString();
}

QString StringBuilder::buildPluginInfoString() const
{
    if (m_violation.type != ViolationType::undefined)
        return buildViolationPluginInfoString();

    return buildErrorPluginInfoString();
}

QString StringBuilder::buildViolationPluginInfoString() const
{
    return lm("Method %1::%2 violated its contract: %3%4").args(
        toHumanReadableString(m_sdkObjectDescription.sdkObjectType()),
        toHumanReadableString(m_sdkMethod),
        toHumanReadableString(m_violation.type),
        m_violation.details.isEmpty() ? QString() : lm(", details: %1").args(m_violation.details));
}

QString StringBuilder::buildErrorPluginInfoString() const
{
    return lm("Method %1::%2 returned an error: %3").args(
        toHumanReadableString(m_sdkObjectDescription.sdkObjectType()),
        toHumanReadableString(m_sdkMethod),
        m_error);
}

QString StringBuilder::buildPluginDiagnosticEventCaption() const
{
    if (m_violation.type != ViolationType::undefined)
        return buildViolationShortString();

    return buildErrorShortString();
}

QString StringBuilder::buildPluginDiagnosticEventDescription() const
{
    if (m_violation.type != ViolationType::undefined)
        return buildViolationFullString();

    return buildErrorFullString();
}

QString StringBuilder::buildViolationFullString() const
{
    return lm("Method %1::%2 of [%3] violated its contract: %4%5").args(
        toHumanReadableString(m_sdkObjectDescription.sdkObjectType()),
        toHumanReadableString(m_sdkMethod),
        m_sdkObjectDescription.descriptionString(),
        toHumanReadableString(m_violation.type),
        m_violation.details.isEmpty() ? QString() : lm(", details: %1").args(m_violation.details));
}

QString StringBuilder::buildViolationShortString() const
{
    return lm("Contract violation: [%1]").args(m_sdkObjectDescription.descriptionString());
}

QString StringBuilder::buildErrorFullString() const
{
    return lm("Method %1::%2 of [%3] returned an error: %4").args(
        toHumanReadableString(m_sdkObjectDescription.sdkObjectType()),
        toHumanReadableString(m_sdkMethod),
        m_sdkObjectDescription.descriptionString(),
        m_error);
}

QString StringBuilder::buildErrorShortString() const
{
    return lm("Error: [%1]").args(m_sdkObjectDescription.descriptionString());
}

} // namespace nx::vms::server::analytics::wrappers
