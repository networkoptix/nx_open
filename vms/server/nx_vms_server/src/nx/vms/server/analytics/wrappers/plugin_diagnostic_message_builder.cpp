#include "plugin_diagnostic_message_builder.h"

#include <nx/utils/switch.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::server::analytics::wrappers {

static QString sdkObjectTypeName(SdkObjectType sdkObjectType)
{
    return QnLexical::serialized(sdkObjectType);
}

static QString violationTypeDescription(ViolationType violationType)
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

        case ViolationType::invalidMetadataTimestamp:
            return "Metadata timestamp must be positive";

        case ViolationType::undefined:
            NX_ASSERT(false, "ViolationType must not be undefined.");
            return "Undefined violation";
    }
    NX_ASSERT(false, "Unknown ViolationType %1.", (int) violationType);
    return "Unknown violation";
}

static QString sdkMethodName(SdkMethod sdkMethod)
{
    if (!NX_ASSERT(sdkMethod != SdkMethod::undefined))
        return "<undefined>"; //< Should clearly differ from any real method.

    return QnLexical::serialized(sdkMethod) + "()";
}

/**
 * @return User-understandable description of the action performed by the given method. Does not
 *     have to be precise - it only must give the user the idea of what is going wrong; the exact
 *     details for technical support must be provided alongside. Must finish the phrase "The plugin
 *     failed to...".
 */
static QString sdkMethodDescription(SdkMethod sdkMethod)
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
        case SdkMethod::iHandler_pushManifest: return "produce updated Manifest";
        case SdkMethod::iHandler_handleMetadata: return "produce valid metadata";

        case SdkMethod::undefined:
            NX_ASSERT(false, "SdkMethod must not be undefined.");
            return "perform an undefined action";
    }
    NX_ASSERT(false, "Unknown SdkMethod %1.", (int) sdkMethod);
    return "perform an unknown action";
}

PluginDiagnosticMessageBuilder::PluginDiagnosticMessageBuilder(
    SdkMethod sdkMethod,
    SdkObjectDescription sdkObjectDescription,
    sdk_support::Error error)
    :
    m_sdkMethod(sdkMethod),
    m_sdkObjectDescription(std::move(sdkObjectDescription)),
    m_error(std::move(error))
{
    NX_ASSERT(m_sdkMethod != SdkMethod::undefined);
    NX_ASSERT(!m_error.isOk());
}

PluginDiagnosticMessageBuilder::PluginDiagnosticMessageBuilder(
    SdkMethod sdkMethod,
    SdkObjectDescription sdkObjectDescription,
    Violation violation)
    :
    m_sdkMethod(sdkMethod),
    m_sdkObjectDescription(std::move(sdkObjectDescription)),
    m_violation(violation)
{
    NX_ASSERT(m_sdkMethod != SdkMethod::undefined);
    NX_ASSERT(m_violation.type != ViolationType::undefined);

    NX_ASSERT(m_error.isOk());
    NX_ASSERT(m_suspicionCaption.isEmpty());
}

PluginDiagnosticMessageBuilder::PluginDiagnosticMessageBuilder(
    SdkObjectDescription sdkObjectDescription,
    QString suspicionCaption,
    QString suspicionDetails)
    :
    m_sdkObjectDescription(std::move(sdkObjectDescription)),
    m_suspicionCaption(std::move(suspicionCaption)),
    m_suspicionDetails(std::move(suspicionDetails))
{
    NX_ASSERT(!m_suspicionCaption.isEmpty());

    NX_ASSERT(m_error.isOk());
    NX_ASSERT(m_sdkMethod == SdkMethod::undefined);
    NX_ASSERT(m_violation.type == ViolationType::undefined);
}

QString PluginDiagnosticMessageBuilder::buildLogString() const
{
    return buildPluginDiagnosticEventDescription();
}

QString PluginDiagnosticMessageBuilder::buildTechnicalDetails() const
{
    if (!m_suspicionCaption.isEmpty())
        return NX_FMT("%1: %2", m_sdkObjectDescription.descriptionString(), m_suspicionDetails);

    NX_ASSERT(m_sdkMethod != SdkMethod::undefined);

    const QString sdkMethodCaption = NX_FMT("Method %1::%2 of %3",
        sdkObjectTypeName(m_sdkObjectDescription.sdkObjectType()),
        sdkMethodName(m_sdkMethod),
        m_sdkObjectDescription.descriptionString());

    if (!m_error.isOk())
        return NX_FMT("%1 returned an error: %2", sdkMethodCaption, m_error);

    NX_ASSERT(m_violation.type != ViolationType::undefined);

    const QString violationDescription = isSdkMethodCallback(m_sdkMethod)
        ? "called by the Plugin incorrectly"
        : "implemented in the Plugin incorrectly";

    return NX_FMT("%1 %2: %3%4",
        sdkMethodCaption,
        violationDescription,
        violationTypeDescription(m_violation.type),
        m_violation.details.isEmpty() ? "" : (", details: " + m_violation.details));
}

QString PluginDiagnosticMessageBuilder::buildPluginDiagnosticEventCaption() const
{
    if (m_violation.type != ViolationType::undefined || !m_suspicionCaption.isEmpty())
        return "Issue with Analytics Plugin detected";

    return "Error in Analytics Plugin detected";
}

QString PluginDiagnosticMessageBuilder::buildPluginDiagnosticEventDescription() const
{
    QString prefix;

    if (m_sdkMethod != SdkMethod::undefined)
        prefix = NX_FMT("Plugin failed to %1. ", sdkMethodDescription(m_sdkMethod));
    else if (!m_suspicionCaption.isEmpty())
        prefix = m_suspicionCaption + " ";

    return NX_FMT("%1Technical details: %2", prefix, buildTechnicalDetails());
}

} // namespace nx::vms::server::analytics::wrappers
