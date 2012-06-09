#ifndef onvif_special_resource_h
#define onvif_special_resource_h

#include "core/resource/network_resource.h"
#include "core/resourcemanagment/resource_searcher.h"

class OnvifSpecialResource
{
public:

    virtual QnNetworkResourcePtr createResource() const = 0;

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters& parameters) = 0;

    virtual QnNetworkResourcePtr createResource(const QByteArray& responseData) const = 0;

    virtual QString manufacture() const = 0;

    virtual ~OnvifSpecialResource() {}
};

class OnvifSpecialResourceCreator
{
public:

    virtual QnNetworkResourcePtr createByManufacturer(const QString& manufacturer) const = 0;

    virtual QnResourcePtr createById(const QnResourceTypePtr& resourceTypePtr, const QnResourceParameters &parameters) const = 0;

    virtual QnNetworkResourcePtr createByPacketData(const QByteArray& packetData, const QString& manufacturer) const = 0;

    virtual ~OnvifSpecialResourceCreator() {}
};

class OnvifSpecialResources: public OnvifSpecialResourceCreator
{
    typedef QHash<QString, OnvifSpecialResource*> ResByManufacturer;
    ResByManufacturer resources;

public:

    OnvifSpecialResources();

    ~OnvifSpecialResources();

    void add(OnvifSpecialResource* resource);

    virtual QnNetworkResourcePtr createByManufacturer(const QString& manufacturer) const;

    virtual QnNetworkResourcePtr createByPacketData(const QByteArray& packetData, const QString& manufacturer) const;

    QnResourcePtr createById(const QnResourceTypePtr& resourceTypePtr, const QnResourceParameters &parameters) const;
};

typedef QSharedPointer<OnvifSpecialResources> OnvifSpecialResourcesPtr;
typedef QSharedPointer<OnvifSpecialResourceCreator> OnvifSpecialResourceCreatorPtr;

#endif // onvif_special_device_h
