#ifndef UPNP_DEVICE_DESCRIPTION_H
#define UPNP_DEVICE_DESCRIPTION_H

#include <QtXml/QXmlDefaultHandler>

namespace nx_upnp {

//Helper formaters "urn:schemas-upnp-org:service:ID:VERSION"
QString toUpnpUrn( const QString& id, const QString& suffix, int version = 1 );
QString fromUpnpUrn( const QString& urn, const QString& suffix, int version = 1 );

//!Contains some info about discovered UPnP device
struct DeviceInfo
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

    std::list<DeviceInfo> deviceList;
    std::list<Service> serviceList;
};

//!Partial parser for SSDP descrition xml (UPnP Device Architecture 1.1, 2.3)
class DeviceDescriptionHandler
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

    const DeviceInfo& deviceInfo() const { return m_deviceInfo; }

private:
    bool charactersInDevice( const QString& ch );
    bool charactersInService( const QString& ch );

private:
    DeviceInfo m_deviceInfo;
    QString m_paramElement;

    std::list<DeviceInfo*> m_deviceStack;
    DeviceInfo::Service* m_lastService;
};

} // namespace nx_upnp

#endif // UPNP_DEVICE_DESCRIPTION_H
