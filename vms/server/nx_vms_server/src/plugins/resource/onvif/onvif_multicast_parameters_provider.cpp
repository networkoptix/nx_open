#include "onvif_multicast_parameters_provider.h"

#include <plugins/resource/onvif/onvif_resource.h>
#include <plugins/resource/onvif/onvif_multicast_parameters_proxy.h>

namespace nx::vms::server::resource {

namespace {

const QString kPrimaryStreamPrefix("Primary");
const QString kSecondaryStreamPrefix("Secondary");

const QString kMulticastAddressPostfix(".multicast.address");
const QString kMulticastPortPostfix(".multicast.port");
const QString kMulticastTtlPostfix(".multicast.ttl");


const QString prefixByStreamIndex(nx::vms::api::StreamIndex streamIndex)
{
    switch (streamIndex)
    {
    case nx::vms::api::StreamIndex::primary:
        return kPrimaryStreamPrefix;
    case nx::vms::api::StreamIndex::secondary:
        return kSecondaryStreamPrefix;
    default:
        return QString();
    }
}

const nx::vms::api::StreamIndex streamIndexByPrefix(const QString& prefix)
{
    if (prefix == kPrimaryStreamPrefix)
        return nx::vms::api::StreamIndex::primary;

    if (prefix == kSecondaryStreamPrefix)
        return nx::vms::api::StreamIndex::secondary;

    return nx::vms::api::StreamIndex::undefined;
}

QnCameraAdvancedParamGroup describeMulticastParameters(const QString& prefix)
{
    QnCameraAdvancedParamGroup streamConfigurationGroup;
    streamConfigurationGroup.name = prefix;

    QnCameraAdvancedParameter address;
    address.id = prefix + kMulticastAddressPostfix;
    address.dataType = QnCameraAdvancedParameter::DataType::String;
    address.name = "Multicast Address";

    QnCameraAdvancedParameter port;
    port.id = prefix + kMulticastPortPostfix;
    port.dataType = QnCameraAdvancedParameter::DataType::Number;
    port.range = "1024,65535";
    port.name = "Port";

    QnCameraAdvancedParameter ttl;
    ttl.id = prefix + kMulticastTtlPostfix;
    ttl.dataType = QnCameraAdvancedParameter::DataType::Number;
    ttl.range = "0,255";
    ttl.name = "TTL";

    streamConfigurationGroup.params.push_back(address);
    streamConfigurationGroup.params.push_back(port);
    streamConfigurationGroup.params.push_back(ttl);

    return streamConfigurationGroup;
}

MulticastParameters fromAdvancedParameters(const QnCameraAdvancedParamValueMap& parameterValues)
{
    MulticastParameters parameters;
    for (auto it = parameterValues.cbegin(); it != parameterValues.cend(); ++it)
    {
        if (const auto parameterId = it.key(); parameterId.endsWith(kMulticastAddressPostfix))
        {
            parameters.address = it.value().toStdString();
        }
        else if (parameterId.endsWith(kMulticastPortPostfix))
        {
            bool success = false;
            const auto port = it.value().toInt(&success);
            if (success)
                parameters.port = port;
        }
        else if (parameterId.endsWith(kMulticastTtlPostfix))
        {
            bool success = false;
            const auto ttl = it.value().toInt(&success);
            if (success)
                parameters.ttl = ttl;
        }
    }

    return parameters;
}

QnCameraAdvancedParamValueMap toAdvancedParameters(
    const QSet<QString> ids,
    nx::vms::api::StreamIndex streamIndex,
    const MulticastParameters& parameters)
{
    if (!NX_ASSERT(!ids.isEmpty()))
        return {};

    const auto prefix = prefixByStreamIndex(streamIndex);
    if (!NX_ASSERT(!prefix.isEmpty()))
        return {};

    QnCameraAdvancedParamValueMap result;
    if (const auto parameterName = prefix + kMulticastAddressPostfix;
        parameters.address && ids.contains(parameterName))
    {
        result[parameterName] = QString::fromStdString(*parameters.address);
    }

    if (const auto parameterName = prefix + kMulticastPortPostfix;
        parameters.port && ids.contains(parameterName))
    {
        result[parameterName] = QString::number(*parameters.port);
    }

    if (const auto parameterName = prefix + kMulticastTtlPostfix;
        parameters.ttl && ids.contains(parameterName))
    {
        result[parameterName] = QString::number(*parameters.ttl);
    }

    return result;
}

} // namespace

OnvifMulticastParametersProvider::OnvifMulticastParametersProvider(
    QnPlOnvifResource* resource,
    nx::vms::api::StreamIndex streamIndex)
    :
    m_resource(resource),
    m_streamIndex(streamIndex)
{
}

QnCameraAdvancedParams OnvifMulticastParametersProvider::descriptions()
{
    QnCameraAdvancedParams result;
    result.name = prefixByStreamIndex(m_streamIndex) + " Multicast Parameters";

    QnCameraAdvancedParamGroup videoStreamsGroup;
    videoStreamsGroup.name = lit("Video Streams Configuration");
    videoStreamsGroup.groups.push_back(
        describeMulticastParameters(prefixByStreamIndex(m_streamIndex)));

    result.groups.push_back(videoStreamsGroup);
    return result;
}

QnCameraAdvancedParamValueMap OnvifMulticastParametersProvider::get(const QSet<QString>& ids)
{
    if (ids.isEmpty())
        return {};

    OnvifMulticastParametersProxy proxy(m_resource, m_streamIndex);
    return toAdvancedParameters(ids, m_streamIndex, proxy.multicastParameters());
}

QSet<QString> OnvifMulticastParametersProvider::set(const QnCameraAdvancedParamValueMap& values)
{
    if (values.isEmpty())
        return {};

    OnvifMulticastParametersProxy proxy(m_resource, m_streamIndex);
    if (proxy.setMulticastParameters(fromAdvancedParameters(values)))
        return values.ids();

    return {};
}

MulticastParameters OnvifMulticastParametersProvider::getMulticastParameters()
{
    OnvifMulticastParametersProxy proxy(m_resource, m_streamIndex);
    return proxy.multicastParameters();
}

bool OnvifMulticastParametersProvider::setMulticastParameters(
    MulticastParameters multicastParameters)
{
    OnvifMulticastParametersProxy proxy(m_resource, m_streamIndex);
    return proxy.setMulticastParameters(std::move(multicastParameters));
}

} // namespace nx::vms::server::resource
