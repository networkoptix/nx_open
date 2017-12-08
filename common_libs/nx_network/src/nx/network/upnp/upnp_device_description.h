#pragma once

#include <QtXml/QXmlDefaultHandler>

namespace nx_upnp {

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

//!Partial parser for SSDP descrition xml (UPnP Device Architecture 1.1, 2.3)
class NX_NETWORK_API DeviceDescriptionHandler
    : public QXmlDefaultHandler
{
public:
    virtual bool startDocument() override;
    virtual bool endDocument() override;

    virtual bool startElement(const QString& namespaceURI,
        const QString& localName,
        const QString& qName,
        const QXmlAttributes& atts) override;

    virtual bool endElement(const QString& namespaceURI,
        const QString& localName,
        const QString& qName) override;

    virtual bool characters(const QString& ch) override;

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

} // namespace nx_upnp

