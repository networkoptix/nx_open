#ifndef QN_RESOURCE_POOL_SCAFFOLD_H
#define QN_RESOURCE_POOL_SCAFFOLD_H

class QnCameraUserAttributePool;
class QnMediaServerUserAttributesPool;

/** RAII class for instantiating all resource pool modules. */
class QnResourcePoolScaffold {
public:
    QnResourcePoolScaffold();
    ~QnResourcePoolScaffold();

private:
    QnCameraUserAttributePool* m_cameraAttributesPool;
    QnMediaServerUserAttributesPool* m_serverAttributesPool;
};

#endif //QN_RESOURCE_POOL_SCAFFOLD_H