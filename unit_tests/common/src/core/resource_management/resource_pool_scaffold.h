#ifndef QN_RESOURCE_POOL_SCAFFOLD_H
#define QN_RESOURCE_POOL_SCAFFOLD_H

class QnCameraUserAttributePool;
class QnMediaServerUserAttributesPool;
class QnResourcePool;

/** RAII class for instantiating all resource pool modules. */
class QnResourcePoolScaffold {
public:
    QnResourcePoolScaffold();
    ~QnResourcePoolScaffold();

    void addCamera();
private:
    QnCameraUserAttributePool* m_cameraAttributesPool;
    QnMediaServerUserAttributesPool* m_serverAttributesPool;
    QnResourcePool* m_resPool;
};

#endif //QN_RESOURCE_POOL_SCAFFOLD_H