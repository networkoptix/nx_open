#ifndef _API_XML_SERIALIZER_H_
#define _API_XML_SERIALIZER_H_

#include "serializer.h"

/**
  * Serialize resource to xml
  */
class QnApiXmlSerializer : public QnApiSerializer
{
public:
    const char* format() const { return "xml"; }

    void deserializeCameras(QnVirtualCameraResourceList& cameras, const QByteArray& data, QnResourceFactory& resourceFactory);
    void deserializeServers(QnVideoServerResourceList& servers, const QByteArray& data);
    void deserializeLayouts(QnLayoutResourceList& layouts, const QByteArray& data);
    void deserializeUsers(QnUserResourceList& users, const QByteArray& data);
    void deserializeResources(QnResourceList& resources, const QByteArray& data, QnResourceFactory& resourceFactory);
    void deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data);

private:
    void serializeCamera(const QnVirtualCameraResourcePtr& resource, QByteArray& data);
    void serializeServer(const QnVideoServerResourcePtr& resource, QByteArray& data);
    void serializeUser(const QnUserResourcePtr& resource, QByteArray& data);
};

#endif // _API_XML_SERIALIZER_H_
