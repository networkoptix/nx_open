#include "upnp_device_description.h"

static const QString urnTemplate = lit("urn:schemas-upnp-org:%1:%2:%3");

namespace nx_upnp {

QString toUpnpUrn(const QString& id, const QString& suffix, int version)
{
    return urnTemplate.arg(suffix).arg(id).arg(version);
}

QString fromUpnpUrn(const QString& urn, const QString& suffix, int version)
{
    const auto split = urn.split(lit(":"));
    if (split.size() == 5
        && split[0] == lit("urn")
        && split[1] == lit("schemas-upnp-org")
        && split[2] == suffix
        && split[4] == QString::number(version))
        return split[3];

    return QString();
}

bool DeviceDescriptionHandler::startDocument()
{
    m_deviceInfo = DeviceInfo();
    m_paramElement.clear();
    m_lastService = 0;
    return true;
}

bool DeviceDescriptionHandler::endDocument()
{
    return true;
}

bool DeviceDescriptionHandler::startElement(
    const QString& /*namespaceURI*/, const QString& /*localName*/,
    const QString& qName, const QXmlAttributes& /*atts*/)
{
    if (qName == lit("device"))
    {
        if (m_deviceStack.empty())
            m_deviceStack.push_back(&m_deviceInfo);
        else
        {
            auto& devs = m_deviceStack.back()->deviceList;
            auto& newDev = *devs.emplace(devs.end());
            m_deviceStack.push_back(&newDev);
        }
    }
    else
        if (qName == lit("service"))
        {
            if (m_deviceStack.empty()) return false;
            auto& servs = m_deviceStack.back()->serviceList;
            auto& newServ = *servs.emplace(servs.end());
            m_lastService = &newServ;
        }
        else
            m_paramElement = qName;
    return true;
}

bool DeviceDescriptionHandler::endElement(
    const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& qName)
{
    if (qName == lit("device"))
        m_deviceStack.pop_back();
    else
        if (qName == lit("service"))
            m_lastService = 0;
        else
            m_paramElement.clear();
    return true;
}

bool DeviceDescriptionHandler::characters(const QString& ch)
{
    if (m_lastService && charactersInService(ch))
        return true;

    if (!m_deviceStack.empty() && charactersInDevice(ch))
        return true;

    return true; // Something not interesting for us
}

bool DeviceDescriptionHandler::charactersInDevice(const QString& ch)
{
    auto& lastDev = *m_deviceStack.back();

    if (m_paramElement == lit("deviceType"))
        lastDev.deviceType = fromUpnpUrn(ch, lit("device"));
    else if (m_paramElement == lit("friendlyName"))
        lastDev.friendlyName = ch;
    else if (m_paramElement == lit("manufacturer"))
        lastDev.manufacturer = ch;
    else if (m_paramElement == lit("manufacturerURL"))
        lastDev.manufacturerUrl = ch;
    else if (m_paramElement == lit("modelName"))
        lastDev.modelName = ch;
    else if (m_paramElement == lit("serialNumber"))
        lastDev.serialNumber = ch;
    else if (m_paramElement == lit("UDN"))
        lastDev.udn = ch;
    else if (m_paramElement == lit("presentationURL"))
        lastDev.presentationUrl = ch.endsWith(lit("/")) ? ch.left(ch.length() - 1) : ch;
    else
        return false; // was not useful

    return true;
}

bool DeviceDescriptionHandler::charactersInService(const QString& ch)
{
    if (m_paramElement == QLatin1String("serviceType"))
        m_lastService->serviceType = fromUpnpUrn(ch, lit("service"));
    else if (m_paramElement == QLatin1String("serviceId"))
        m_lastService->serviceId = ch;
    else if (m_paramElement == QLatin1String("controlURL"))
        m_lastService->controlUrl = ch;
    else if (m_paramElement == QLatin1String("eventSubURL"))
        m_lastService->eventSubUrl = ch;
    else if (m_paramElement == QLatin1String("SCPDURL"))
        m_lastService->scpdUrl = ch;
    else
        return false; // was not useful

    return true;
}

} // namespace nx_upnp
