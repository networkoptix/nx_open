#ifndef QN_CLIENT_MODULE_H
#define QN_CLIENT_MODULE_H

#include <memory>

#include <QtCore/QObject>

class QnCameraUserAttributePool;
class QnMediaServerUserAttributesPool;

class QnClientModule: public QObject {
    Q_OBJECT
public:
    QnClientModule(bool forceLocalSettings = false, QObject *parent = NULL);
    virtual ~QnClientModule();

private:
    std::unique_ptr<QnCameraUserAttributePool> m_cameraUserAttributePool;
    std::unique_ptr<QnMediaServerUserAttributesPool> m_mediaServerUserAttributesPool;
};


#endif // QN_CLIENT_MODULE_H
