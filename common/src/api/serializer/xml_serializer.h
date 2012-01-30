#ifndef _API_XML_SERIALIZER_H_
#define _API_XML_SERIALIZER_H_

#include "serializer.h"

/**
  * Serialize resource to xml
  */
class QnApiXmlSerializer : public QnApiSerializer
{
public:
    void serialize(const QnResourcePtr& resource, QByteArray& data);

    void deserializeStorages(QnStorageList& storages, const QByteArray& data, QnResourceFactory& resourceFactory);
    void deserializeCameras(QnCameraResourceList& cameras, const QByteArray& data, QnResourceFactory& resourceFactory);
    void deserializeServers(QnVideoServerList& servers, const QByteArray& data);
    void deserializeLayouts(QnLayoutDataList& layouts, const QByteArray& data);
    void deserializeUsers(QnUserResourceList& users, const QByteArray& data);
    void deserializeResources(QnResourceList& resources, const QByteArray& data, QnResourceFactory& resourceFactory);
    void deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data);

private:
    void serializeStorage(const QnStoragePtr& resource, QByteArray& data);
    void serializeCamera(const QnCameraResourcePtr& resource, QByteArray& data);
    void serializeServer(const QnVideoServerPtr& resource, QByteArray& data);
    void serializeUser(const QnUserResourcePtr& resource, QByteArray& data);
};

#endif // _API_XML_SERIALIZER_H_
