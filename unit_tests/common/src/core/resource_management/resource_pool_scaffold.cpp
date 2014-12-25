#include "resource_pool_scaffold.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/media_server_user_attributes.h>

QnResourcePoolScaffold::QnResourcePoolScaffold() {
    m_cameraAttributesPool = new QnCameraUserAttributePool();
    m_serverAttributesPool = new QnMediaServerUserAttributesPool();
    QnResourcePool::initStaticInstance( new QnResourcePool() );
    QnResourcePool::instance(); // to initialize net state;
}

QnResourcePoolScaffold::~QnResourcePoolScaffold() {
    delete QnResourcePool::instance();
    QnResourcePool::initStaticInstance( NULL );
    delete m_serverAttributesPool;
    delete m_cameraAttributesPool;
}
