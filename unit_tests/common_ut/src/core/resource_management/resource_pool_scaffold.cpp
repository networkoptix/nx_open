#include "resource_pool_scaffold.h"

#include <common/common_meta_types.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>
#include <core/resource/camera_user_attribute_pool.h>
#include <core/resource/media_server_user_attributes.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_resource_stub.h>
#include <core/resource/media_server_resource.h>

QnResourcePoolScaffold::QnResourcePoolScaffold() {
    QnCommonMetaTypes::initialize();

    m_propertyDictionary = new QnResourcePropertyDictionary();
    m_statusDictionary = new QnResourceStatusDictionary();
    m_cameraAttributesPool = new QnCameraUserAttributePool();
    m_serverAttributesPool = new QnMediaServerUserAttributesPool();
    m_resPool = new QnResourcePool();
}

QnResourcePoolScaffold::~QnResourcePoolScaffold() {
    delete m_resPool;
    delete m_serverAttributesPool;
    delete m_cameraAttributesPool;
    delete m_statusDictionary;
    delete m_propertyDictionary;
}

QnVirtualCameraResourcePtr QnResourcePoolScaffold::addCamera(Qn::LicenseType cameraType, bool licenseRequired) {
    return addCameras(cameraType, 1, licenseRequired).first();
}

QnVirtualCameraResourceList QnResourcePoolScaffold::addCameras(Qn::LicenseType cameraType /*= Qn::LC_Professional*/, int count /*= 1*/, bool licenseRequired /*= true*/) {
    if (m_resPool->getAllServers().isEmpty()) {
        QnMediaServerResourcePtr server(new QnMediaServerResource(qnResTypePool));
        server->setId(QnUuid::createUuid());
        server->setStatus(Qn::Online, true);
        m_resPool->addResource(server);
    }

    QnVirtualCameraResourceList result;
    for (int i = 0; i < count; ++i) {
        QnVirtualCameraResourcePtr camera(new QnCameraResourceStub(cameraType));
        camera->setParentId(m_resPool->getAllServers().first()->getId());
        camera->setLicenseUsed(licenseRequired);
        m_resPool->addResource(camera);
        result << camera;
    }
    return result;
}
