#include "string_builder.h"

#include <nx/utils/switch.h>

#include <nx/vms/server/sdk_support/to_string.h>
#include <nx/fusion/model_functions.h>

namespace nx::vms::server::analytics::wrappers {

static QString toString(SdkObjectType sdkObjectType)
{
    return QnLexical::serialized(sdkObjectType);
}

static QString toString(SdkMethod sdkMethod)
{
    return QnLexical::serialized(sdkMethod) + "()";
}

static QString toString(ViolationType violationType)
{
    switch (violationType)
    {
        case ViolationType::nullManifest:
            return "Manifest is null";
        case ViolationType::nullManifestString:
            return "Manifest string is null";
        case ViolationType::emptyManifestString:
            return "Manifest string is empty";
        case ViolationType::invalidJson:
            return "Manifest deserialization error - manifest is not a valid JSON";
        case ViolationType::invalidJsonStructure:
            return "Manifest deserialization error - "
                "unable to deserialize the provided JSON to the Manifest structure";
        case ViolationType::manifestLogicalError:
            return "Manifest contains logical errors";
        case ViolationType::nullEngine:
            return "Engine is null while no error present";
        case ViolationType::inconsistentActionResult:
            return "Action result is inconsistent: more than one exclusive value returned";
        case ViolationType::invalidActionResultUrl:
            return "Action result contains invalid URL";
        case ViolationType::methodExecutionTookTooLong:
            return "Method execution took too long";

        case ViolationType::undefined:
            NX_ASSERT(false, "ViolationType must not be undefined.");
            return "Undefined violation";
    }
    NX_ASSERT(false, "Unknown ViolationType %1.", (int) violationType);
    return "Unknown violation";
}

/**
 * @return User-understandable description of the action performed by the given method. Does not
 *     have to be precise - it only must give the user the idea of what is going wrong; the exact
 *     details for technical support must be provided alongside.
 */
static QString methodDescription(SdkMethod sdkMethod)
{
    switch (sdkMethod)
    {
        case SdkMethod::manifest: return "produce Manifest";
        case SdkMethod::setSettings: return "accept Settings";
        case SdkMethod::pluginSideSettings: return "provide Settings";
        case SdkMethod::setHandler: return "initialize";
        case SdkMethod::createEngine: return "start";
        case SdkMethod::setEngineInfo: return "accept info";
        case SdkMethod::isCompatible: return "inform about Camera/Device compatibility";
        case SdkMethod::obtainDeviceAgent: return "start working with Camera/Device";
        case SdkMethod::executeAction: return "execute Action";
        case SdkMethod::setNeededMetadataTypes: return "accept Metadata types";
        case SdkMethod::pushDataPacket: return "accept audio/video/metadata";
        case SdkMethod::pushManifest: return "produce updated Manifest";

        case SdkMethod::undefined:
            NX_ASSERT(false, "SdkMethod must not be undefined.");
            return "perform an undefined action";
    }
    NX_ASSERT(false, "Unknown SdkMethod %1.", (int) sdkMethod);
    return "perform an unknown action";
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
    NX_ASSERT(!m_error.isOk());
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
    NX_ASSERT(m_violation.type != ViolationType::undefined);
}

QString StringBuilder::buildLogString() const
{
    return buildPluginDiagnosticEventDescription();
}

QString StringBuilder::buildPluginInfoString() const
{
    const QString methodCaption = NX_FMT("Method %1::%2 of %3",
        m_sdkObjectDescription.sdkObjectType(),
        m_sdkMethod,
        m_sdkObjectDescription.descriptionString());

    if (m_violation.type != ViolationType::undefined)
    {
        return NX_FMT("%1 violated its contract: %2%3",
            methodCaption,
            m_violation.type,
            m_violation.details.isEmpty() ? "" : (", details: " + m_violation.details));
    }

    return NX_FMT("%1 returned an error: %2", methodCaption, m_error);
}

QString StringBuilder::buildPluginDiagnosticEventCaption() const
{
    if (m_violation.type != ViolationType::undefined)
        return "Issue with Analytics Plugin detected";

    return "Error in Analytics Plugin detected";
}

QString StringBuilder::buildPluginDiagnosticEventDescription() const
{
    const QString prefix = (m_sdkMethod == SdkMethod::undefined)
        ? ""
        : NX_FMT("Plugin cannot %1. ", methodDescription(m_sdkMethod));

    return prefix + "Technical details: " + buildPluginInfoString();
}

} // namespace nx::vms::server::analytics::wrappers
