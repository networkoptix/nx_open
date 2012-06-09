#include "onvif_special_resource.h"
#include <QDebug>


OnvifSpecialResources::OnvifSpecialResources()
{

}

OnvifSpecialResources::~OnvifSpecialResources()
{

}

void OnvifSpecialResources::add(OnvifSpecialResource* resource)
{
    QString manufacturer = resource->manufacture().toLower();
    if (resources.contains(manufacturer)) {
        qWarning() << "OnvifSpecialResources::add: resource for " << manufacturer << " already added.";
        return;
    }

    resources.insert(manufacturer, resource);
}

QnNetworkResourcePtr OnvifSpecialResources::createByManufacturer(const QString& manufacturer) const
{
    QString key = manufacturer.toLower();
    ResByManufacturer::ConstIterator resourceIter = resources.find(key);

    QnNetworkResourcePtr result(0);

    if (resourceIter != resources.end()) {
        result = resourceIter.value()->createResource();
    }

    if (result.isNull()) {
        qWarning() << "OnvifSpecialResources::createByManufacturer: can't create resource for manufacturer: " << manufacturer;
    }

    return result;
}

QnResourcePtr OnvifSpecialResources::createById(const QnResourceTypePtr& resourceTypePtr, const QnResourceParameters &parameters) const
{
    QnResourcePtr result(0);
    if (resourceTypePtr.isNull()) {
        qDebug() << "OnvifSpecialResources::createById: got empty resource type pointer";
        return result;
    }

    QString key = resourceTypePtr->getManufacture().toLower();
    ResByManufacturer::ConstIterator resourceIter = resources.find(key);

    if (resourceIter != resources.end()) {
        result = resourceIter.value()->createResource(resourceTypePtr->getId(), parameters);
    }

    if (result.isNull()) {
        qDebug() << "OnvifSpecialResources::createById: can't create resource for typeId: "
                 << resourceTypePtr->getId().toString();
    }

    return result;
}

QnNetworkResourcePtr OnvifSpecialResources::createByPacketData(const QByteArray& packetData, const QString& manufacturer) const
{
    QString key = manufacturer.toLower();
    ResByManufacturer::ConstIterator resourceIter = resources.find(key);

    QnNetworkResourcePtr result(0);

    if (resourceIter != resources.end()) {
        result = resourceIter.value()->createResource(packetData);
    }

    if (result.isNull()) {
        qWarning() << "OnvifSpecialResources::createByPacketData: can't create resource for manufacturer: " << manufacturer;
    }

    return result;
}
