#include "camera_user_attribute_pool.h"

QnCameraUserAttributePool::QnCameraUserAttributePool(QObject *parent):
    QObject(parent),
    QnCommonModuleAware(parent)
{
    setElementInitializer( []( const QnUuid& cameraId, QnCameraUserAttributesPtr& userAttributes ){
        userAttributes = QnCameraUserAttributesPtr( new QnCameraUserAttributes() );
        userAttributes->cameraId = cameraId;
    } );
}

QnCameraUserAttributePool::~QnCameraUserAttributePool()
{}

QnCameraUserAttributesList QnCameraUserAttributePool::getAttributesList( const QList<QnUuid>& idList )
{
    QnCameraUserAttributesList valList;
    valList.reserve( idList.size() );
    for( const QnUuid id: idList )
        valList.push_back( get(id) );
    return valList;
}
