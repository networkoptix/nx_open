// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QXmlStreamReader>

namespace nx::network::upnp {

// Basic UPnP device type.
static const QString kBasicDeviceType = "Basic";

//Helper formaters "urn:schemas-upnp-org:service:ID:VERSION"
QString NX_NETWORK_API toUpnpUrn(const QString& id, const QString& suffix, int version = 1);
QString NX_NETWORK_API fromUpnpUrn(const QString& urn, const QString& suffix, int version = 1);

//!Contains some info about discovered UPnP device
struct NX_NETWORK_API DeviceInfo
{
    QString deviceType;
    QString friendlyName;
    QString manufacturer;
    QString manufacturerUrl;
    QString modelName;
    QString serialNumber;
    QString udn;
    QString presentationUrl;

    struct Service
    {
        QString serviceType;
        QString serviceId;
        QString controlUrl;
        QString eventSubUrl;
        QString scpdUrl;
    };

    std::list<DeviceInfo> deviceList;
    std::list<Service> serviceList;
};

//!Partial parser for SSDP description xml (UPnP Device Architecture 1.1, 2.3)
class NX_NETWORK_API DeviceDescriptionHandler
{
public:
    virtual bool startDocument();
    virtual bool endDocument();

    virtual bool startElement(
        const QStringView& namespaceUri,
        const QStringView& localName,
        const QXmlStreamAttributes& atts);

    virtual bool endElement(
        const QStringView& namespaceURI,
        const QStringView& localName);

    virtual bool characters(const QStringView& ch);

    const DeviceInfo& deviceInfo() const { return m_deviceInfo; }

private:
    bool charactersInDevice(const QString& ch);
    bool charactersInService(const QString& ch);

private:
    DeviceInfo m_deviceInfo;
    QString m_paramElement;

    std::list<DeviceInfo*> m_deviceStack;
    DeviceInfo::Service* m_lastService;
};

} // namespace nx::network::upnp
