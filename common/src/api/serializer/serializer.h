#ifndef _API_SERIALIZER_H_
#define _API_SERIALIZER_H_

#include <QByteArray>

#include "core/resource/resource.h"
#include "core/resource/camera_resource.h"
#include "core/resource/video_server.h"
#include "core/resource/layout_resource.h"
#include "core/resource/storage_resource.h"
#include "core/resource/user_resource.h"

void parseRegion(QRegion& region, const QString& regionString);
QString serializeRegion(const QRegion& region);

class QnSerializeException : public std::exception
{
public:
    QnSerializeException(const QByteArray& errorString)
        : m_errorString(errorString)
    {
    }

    ~QnSerializeException() throw() {}

    const QByteArray& errorString() const { return m_errorString; }

private:
    QByteArray m_errorString;
};

/**
  * Serialize resource
  */
class QnApiSerializer
{
public:
    void serialize(const QnResourcePtr& resource, QByteArray& data);

    virtual void deserializeCameras(QnVirtualCameraResourceList& cameras, const QByteArray& data, QnResourceFactory& resourceFactory) = 0;
    virtual void deserializeServers(QnVideoServerResourceList& servers, const QByteArray& data) = 0;
    virtual void deserializeLayouts(QnLayoutResourceList& layouts, const QByteArray& data) = 0;
    virtual void deserializeUsers(QnUserResourceList& users, const QByteArray& data) = 0;
    virtual void deserializeResources(QnResourceList& resources, const QByteArray& data, QnResourceFactory& resourceFactory) = 0;
    virtual void deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data) = 0;

protected:
    virtual void serializeCamera(const QnVirtualCameraResourcePtr& resource, QByteArray& data) = 0;
    virtual void serializeServer(const QnVideoServerResourcePtr& resource, QByteArray& data) = 0;
    virtual void serializeUser(const QnUserResourcePtr& resource, QByteArray& data) = 0;
};

#endif // _API_SERIALIZER_H_
