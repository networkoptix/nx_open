#ifndef onvif_special_resource_h
#define onvif_special_resource_h

#include "core/resource/network_resource.h"
#include "core/resourcemanagment/resource_searcher.h"

class OnvifSpecialResource
{
public:

    virtual QnNetworkResourcePtr createResource() const = 0;

    virtual QnResourcePtr createResource(QnId /*resourceTypeId*/, const QnResourceParameters& /*parameters*/) = 0;

    virtual QString manufacture() const = 0;

    virtual ~OnvifSpecialResource() = 0;
};

class OnvifSpecialResourceCreator
{
public:

    virtual QnNetworkResourcePtr createByManufacturer(const QString& manufacturer) const = 0;

    virtual QnResourcePtr createById(QnId resourceTypeId, const QnResourceParameters &parameters) const = 0;

    virtual ~OnvifSpecialResourceCreator();
};

typedef QSharedPointer<OnvifSpecialResource> OnvifSpecialResourcePtr;

class OnvifSpecialResources: public OnvifSpecialResourceCreator
{
    typedef QHash<QString, OnvifSpecialResourcePtr> ResByManufacturer;
    ResByManufacturer resources;

public:

    void add(const OnvifSpecialResourcePtr& resource);

    virtual QnNetworkResourcePtr createByManufacturer(const QString& manufacturer) const;

    QnResourcePtr createById(QnId resourceTypeId, const QnResourceParameters &parameters) const;
};

#endif // onvif_special_device_h
