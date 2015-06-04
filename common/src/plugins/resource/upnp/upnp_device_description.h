#ifndef UPNP_DEVICE_DESCRIPTION_H
#define UPNP_DEVICE_DESCRIPTION_H

#include <QtXml/QXmlDefaultHandler>

//!Contains some info about discovered UPnP device
struct UpnpDeviceInfo
{
    QString deviceType;
    QString friendlyName;
    QString manufacturer;
    QString modelName;
    QString serialNumber;
    QString presentationUrl;

    struct Service
    {
        QString serviceType;
        QString serviceId;
        QString controlUrl;
        QString eventSubUrl;
        QString scpdUrl;
    };

    std::list<UpnpDeviceInfo> deviceList;
    std::list<Service> serviceList;
};

//!Partial parser for SSDP descrition xml (UPnP Device Architecture 1.1, 2.3)
class UpnpDeviceDescriptionHandler
    : public QXmlDefaultHandler
{
public:
    virtual bool startDocument() override;
    virtual bool endDocument() override;

    virtual bool startElement( const QString& namespaceURI,
                               const QString& localName,
                               const QString& qName,
                               const QXmlAttributes& atts ) override;

    virtual bool endElement( const QString& namespaceURI,
                             const QString& localName,
                             const QString& qName ) override;

    virtual bool characters( const QString& ch ) override;

    const UpnpDeviceInfo& deviceInfo() const { return m_deviceInfo; }

private:
    bool charactersInDevice( const QString& ch );
    bool charactersInService( const QString& ch );

private:
    UpnpDeviceInfo m_deviceInfo;
    QString m_paramElement;

    std::list<UpnpDeviceInfo*> m_deviceStack;
    UpnpDeviceInfo::Service* m_lastService;
};

#endif // UPNP_DEVICE_DESCRIPTION_H
