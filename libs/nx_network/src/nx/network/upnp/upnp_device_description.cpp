// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "upnp_device_description.h"

namespace nx::network::upnp {

namespace {

static const QString kUrnTemplate = "urn:schemas-upnp-org:%1:%2:%3";

} // namespace

QString toUpnpUrn(const QString& id, const QString& suffix, int version)
{
    return kUrnTemplate.arg(suffix, id, QString::number(version));
}

QString fromUpnpUrn(const QString& urn, const QString& suffix, int version)
{
    const auto split = urn.split(":");
    if (split.size() == 5
        && split[0] == "urn"
        && split[1] == "schemas-upnp-org"
        && split[2] == suffix
        && split[4] == QString::number(version))
        return split[3];

    return QString();
}

bool DeviceDescriptionHandler::startDocument()
{
    m_deviceStack.clear();
    m_deviceInfo = DeviceInfo();
    m_paramElement.clear();
    m_lastService = nullptr;
    return true;
}

bool DeviceDescriptionHandler::endDocument()
{
    return true;
}

bool DeviceDescriptionHandler::startElement(
    const QStringView& /*namespaceUri*/,
    const QStringView& localName,
    const QXmlStreamAttributes& /*atts*/)
{
    if (localName == QLatin1String("device"))
    {
        if (m_deviceStack.empty())
        {
            m_deviceStack.push_back(&m_deviceInfo);
        }
        else
        {
            auto& devs = m_deviceStack.back()->deviceList;
            auto& newDev = *devs.emplace(devs.end());
            m_deviceStack.push_back(&newDev);
        }
    }
    else if (localName == QLatin1String("service"))
    {
        if (m_deviceStack.empty())
            return false;
        auto& servs = m_deviceStack.back()->serviceList;
        auto& newServ = *servs.emplace(servs.end());
        m_lastService = &newServ;
    }
    else
    {
        m_paramElement = localName.toString();
    }

    return true;
}

bool DeviceDescriptionHandler::endElement(
    const QStringView& /*namespaceURI*/,
    const QStringView& localName)
{
    if (localName == QLatin1String("device"))
        m_deviceStack.pop_back();
    else if (localName == QLatin1String("service"))
        m_lastService = nullptr;
    else
        m_paramElement.clear();

    return true;
}

bool DeviceDescriptionHandler::characters(const QStringView& ch)
{
    if (m_lastService && charactersInService(ch.toString()))
        return true;

    if (!m_deviceStack.empty() && charactersInDevice(ch.toString()))
        return true;

    return true; //< Something not interesting for us.
}

bool DeviceDescriptionHandler::charactersInDevice(const QString& ch)
{
    auto& lastDev = *m_deviceStack.back();

    if (m_paramElement == "deviceType")
        lastDev.deviceType = fromUpnpUrn(ch, "device");
    else if (m_paramElement == "friendlyName")
        lastDev.friendlyName = ch;
    else if (m_paramElement == "manufacturer")
        lastDev.manufacturer = ch;
    else if (m_paramElement == "manufacturerURL")
        lastDev.manufacturerUrl = ch;
    else if (m_paramElement == "modelName")
        lastDev.modelName = ch;
    else if (m_paramElement == "serialNumber")
        lastDev.serialNumber = ch;
    else if (m_paramElement == "UDN")
        lastDev.udn = ch;
    else if (m_paramElement == "presentationURL")
        lastDev.presentationUrl = ch.endsWith("/") ? ch.left(ch.length() - 1) : ch;
    else
        return false; //< Was not useful.

    return true;
}

bool DeviceDescriptionHandler::charactersInService(const QString& ch)
{
    if (m_paramElement == QLatin1String("serviceType"))
        m_lastService->serviceType = fromUpnpUrn(ch, "service");
    else if (m_paramElement == QLatin1String("serviceId"))
        m_lastService->serviceId = ch;
    else if (m_paramElement == QLatin1String("controlURL"))
        m_lastService->controlUrl = ch;
    else if (m_paramElement == QLatin1String("eventSubURL"))
        m_lastService->eventSubUrl = ch;
    else if (m_paramElement == QLatin1String("SCPDURL"))
        m_lastService->scpdUrl = ch;
    else
        return false; //< Was not useful.

    return true;
}

} // namespace nx::network::upnp
