#ifndef QN_RESOURCE_POOL_SCAFFOLD_H
#define QN_RESOURCE_POOL_SCAFFOLD_H

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>

class QnResourcePropertyDictionary;
class QnResourceStatusDictionary;
class QnCameraUserAttributePool;
class QnMediaServerUserAttributesPool;
class QnResourcePool;

/** RAII class for instantiating all resource pool modules. */
class QnResourcePoolScaffold {
public:
    QnResourcePoolScaffold();
    ~QnResourcePoolScaffold();

    QnVirtualCameraResourcePtr addCamera(Qn::LicenseType cameraType = Qn::LC_Professional);
private:
    QnResourcePropertyDictionary* m_propertyDictionary;
    QnResourceStatusDictionary* m_statusDictionary;
    QnCameraUserAttributePool* m_cameraAttributesPool;
    QnMediaServerUserAttributesPool* m_serverAttributesPool;
    QnResourcePool* m_resPool;
};

#endif //QN_RESOURCE_POOL_SCAFFOLD_H