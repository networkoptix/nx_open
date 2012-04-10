#ifndef _API_PB_SERIALIZER_H_
#define _API_PB_SERIALIZER_H_

#include "serializer.h"

/**
  * Serialize resource to protobuf
  */
class QnApiPbSerializer : public QnApiSerializer
{
public:
    const char* format() const { return "pb"; }

    void deserializeCameras(QnVirtualCameraResourceList& cameras, const QByteArray& data, QnResourceFactory& resourceFactory);
    void deserializeServers(QnVideoServerResourceList& servers, const QByteArray& data, QnResourceFactory& resourceFactory);
    void deserializeLayouts(QnLayoutResourceList& layouts, const QByteArray& data);
    void deserializeUsers(QnUserResourceList& users, const QByteArray& data);
    void deserializeResources(QnResourceList& resources, const QByteArray& data, QnResourceFactory& resourceFactory);
    void deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data);
    void deserializeLicenses(QnLicenseList& licenses, const QByteArray& data);

    void serializeLayouts(const QnLayoutResourceList& layouts, QByteArray& data);
    void serializeCameras(const QnVirtualCameraResourceList& cameras, QByteArray& data);
    void serializeLicense(const QnLicensePtr& license, QByteArray& data);
private:
    void serializeCamera(const QnVirtualCameraResourcePtr& resource, QByteArray& data);
    void serializeServer(const QnVideoServerResourcePtr& resource, QByteArray& data);
    void serializeUser(const QnUserResourcePtr& resource, QByteArray& data);
    void serializeLayout(const QnLayoutResourcePtr& resource, QByteArray& data);

};

#endif // _API_PB_SERIALIZER_H_
