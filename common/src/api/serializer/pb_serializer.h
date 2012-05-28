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

    void deserializeCameras(QnVirtualCameraResourceList& cameras, const QByteArray& data, QnResourceFactory& resourceFactory) override;
    void deserializeServers(QnVideoServerResourceList& servers, const QByteArray& data, QnResourceFactory& resourceFactory) override;
    void deserializeLayout(QnLayoutResourcePtr& layout, const QByteArray& data) override;
    void deserializeLayouts(QnLayoutResourceList& layouts, const QByteArray& data) override;
    void deserializeUsers(QnUserResourceList& users, const QByteArray& data) override;
    void deserializeResources(QnResourceList& resources, const QByteArray& data, QnResourceFactory& resourceFactory) override;
    void deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data) override;
    void deserializeLicenses(QnLicenseList& licenses, const QByteArray& data) override;
    void deserializeCameraHistoryList(QnCameraHistoryList& cameraServerItems, const QByteArray& data) override;

    void serializeLayouts(const QnLayoutResourceList& layouts, QByteArray& data) override;
    void serializeLayout(const QnLayoutResourcePtr& resource, QByteArray& data) override;
    void serializeCameras(const QnVirtualCameraResourceList& cameras, QByteArray& data) override;
    void serializeLicense(const QnLicensePtr& license, QByteArray& data) override;
    void serializeCameraServerItem(const QnCameraHistoryItem& cameraHistory, QByteArray& data) override;

private:
    void serializeCamera(const QnVirtualCameraResourcePtr& resource, QByteArray& data) override;
    void serializeServer(const QnVideoServerResourcePtr& resource, QByteArray& data) override;
    void serializeUser(const QnUserResourcePtr& resource, QByteArray& data) override;
};

#endif // _API_PB_SERIALIZER_H_
