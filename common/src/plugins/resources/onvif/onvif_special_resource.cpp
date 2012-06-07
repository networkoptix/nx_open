#include "onvif_special_resource.h"
#include <QDebug>


void OnvifSpecialResources::add(const OnvifSpecialResourcePtr& resource)
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

QnResourcePtr OnvifSpecialResources::createById(QnId resourceTypeId, const QnResourceParameters &parameters) const
{
    QnResourcePtr result(0);
    QnResourceTypePtr resourceTypePtr = qnResTypePool->getResourceType(resourceTypeId);
    if (resourceTypePtr.isNull()) {
        qDebug() << "OnvifSpecialResources::createById: can't create resource for typeId: " << resourceTypeId.toString();
        return result;
    }

    QString key = resourceTypePtr->getManufacture().toLower();
    ResByManufacturer::ConstIterator resourceIter = resources.find(key);

    if (resourceIter != resources.end()) {
        result = resourceIter.value()->createResource(resourceTypeId, parameters);
    }

    if (result.isNull()) {
        qDebug() << "OnvifSpecialResources::createById: can't create resource for typeId: " << resourceTypeId.toString();
    }

    return result;
}
