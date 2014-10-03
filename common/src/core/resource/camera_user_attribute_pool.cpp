/**********************************************************
* 1 oct 2014
* akolesnikov
***********************************************************/

#include "camera_user_attribute_pool.h"


////////////////////////////////////////////////////////////
//// QnCameraUserAttributePool
////////////////////////////////////////////////////////////

static QnCameraUserAttributePool* QnCameraUserAttributePool_instance = nullptr;

QnCameraUserAttributePool::QnCameraUserAttributePool()
{
    QnCameraUserAttributePool_instance = this;
    setElementInitializer( []( const QnUuid& cameraID, QnCameraUserAttributesPtr& userAttributes ){
        userAttributes = QnCameraUserAttributesPtr( new QnCameraUserAttributes() );
        userAttributes->cameraID = cameraID;
    } );
}

QnCameraUserAttributePool::~QnCameraUserAttributePool()
{
    assert( QnCameraUserAttributePool_instance == this );
    QnCameraUserAttributePool_instance = nullptr;
}

QnCameraUserAttributesList QnCameraUserAttributePool::getAttributesList( const QList<QnUuid>& idList )
{
    QnCameraUserAttributesList valList;
    valList.reserve( idList.size() );
    for( const QnUuid id: idList )
        valList.push_back( get(id) );
    return valList;
}

QnCameraUserAttributePool* QnCameraUserAttributePool::instance()
{
    return QnCameraUserAttributePool_instance;
}
