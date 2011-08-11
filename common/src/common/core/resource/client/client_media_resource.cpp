#include "client_media_resource.h"
#include "resources/client/rtsp_client_dataprovider.h"
#include "resourcecontrol/resource_pool.h"

QnClientMediaResource::QnClientMediaResource(): QnMediaResource()
{

}

QnClientMediaResource::~QnClientMediaResource()
{

}

QnAbstractMediaStreamDataProvider* QnClientMediaResource::createMediaProvider()
{
    return new QnRtspClientDataProvider(QnResourcePool::instance().getResourceById(getId()));
}
