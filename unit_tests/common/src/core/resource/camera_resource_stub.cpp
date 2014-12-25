#include "camera_resource_stub.h"

QnCameraResourceStub::QnCameraResourceStub()
{
    setId(QnUuid::createUuid());
    //setTypeId(qnResTypePool->);
    addFlags(Qn::server_live_cam);
}

QString QnCameraResourceStub::getDriverName() const {
    return lit("QnCameraResourceStub");
}

void QnCameraResourceStub::setIframeDistance(int /*frames*/ , int /*timems*/ ) {
}

QnAbstractStreamDataProvider * QnCameraResourceStub::createLiveDataProvider() {
    return NULL;
}

Qn::ResourceStatus QnCameraResourceStub::getStatus() const {
    return Qn::Online;
}
