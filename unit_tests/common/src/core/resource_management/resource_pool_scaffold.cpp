#include "resource_pool_scaffold.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/media_server_user_attributes.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_resource_stub.h>
#include <core/resource/media_server_resource.h>

QnResourcePoolScaffold::QnResourcePoolScaffold() {
    m_cameraAttributesPool = new QnCameraUserAttributePool();
    m_serverAttributesPool = new QnMediaServerUserAttributesPool();
    m_resPool = new QnResourcePool();
    QnResourcePool::initStaticInstance(m_resPool);
    QnResourcePool::instance(); // to initialize net state;
}

QnResourcePoolScaffold::~QnResourcePoolScaffold() {
    delete m_resPool;
    QnResourcePool::initStaticInstance( NULL );
    delete m_serverAttributesPool;
    delete m_cameraAttributesPool;
}

void QnResourcePoolScaffold::addCamera() {
    if (m_resPool->getAllServers().isEmpty()) {
        QnMediaServerResourcePtr server(new QnMediaServerResource(qnResTypePool));
        server->setId(QnUuid::createUuid());
        m_resPool->addResource(server);
    }

    QnVirtualCameraResourcePtr camera(new QnCameraResourceStub());
    camera->setParentId(m_resPool->getAllServers().first()->getId());
    m_resPool->addResource(camera);
}
