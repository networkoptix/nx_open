#ifndef _API_SERIALIZER_H_
#define _API_SERIALIZER_H_

#include <QByteArray>

#include "core/resource/resource.h"
#include "core/resource/camera_resource.h"
#include "core/resource/video_server.h"
#include "core/resource/layout_data.h"
#include "core/resource/qnstorage.h"
#include "core/resource/user.h"

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
    virtual void serialize(const QnResourcePtr& resource, QByteArray& data) = 0;

    virtual void deserializeStorages(QnStorageList& storages, const QByteArray& data, QnResourceFactory& resourceFactory) = 0;
    virtual void deserializeCameras(QnCameraResourceList& cameras, const QByteArray& data, QnResourceFactory& resourceFactory) = 0;
    virtual void deserializeServers(QnVideoServerList& servers, const QByteArray& data) = 0;
    virtual void deserializeLayouts(QnLayoutDataList& layouts, const QByteArray& data) = 0;
    virtual void deserializeUsers(QnUserResourceList& users, const QByteArray& data) = 0;
    virtual void deserializeResources(QnResourceList& resources, const QByteArray& data, QnResourceFactory& resourceFactory) = 0;
    virtual void deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data) = 0;
};

#endif // _API_SERIALIZER_H_
