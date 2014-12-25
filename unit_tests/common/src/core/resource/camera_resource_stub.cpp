#include "camera_resource_stub.h"

QnCameraResourceStub::QnCameraResourceStub(Qn::LicenseType cameraType):
    m_cameraType(cameraType)
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

Qn::LicenseType QnCameraResourceStub::licenseType() const {
    return m_cameraType;
}
