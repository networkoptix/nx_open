#include "sony_resource.h"
#include "onvif/soapMediaBindingProxy.h"


int QnPlSonyResource::MAX_RESOLUTION_DECREASES_NUM = 3;

QnPlSonyResource::QnPlSonyResource()
{

}

bool QnPlSonyResource::updateResourceCapabilities()
{
    if (!QnPlOnvifResource::updateResourceCapabilities()) {
        return false;
    }

    std::string confToken = getPrimaryVideoEncoderId().toStdString();
    if (confToken.empty()) {
        return false;
    }

    QAuthenticator auth(getAuth());
    std::string login = auth.user().toStdString();
    std::string password = auth.password().toStdString();
    std::string endpoint = getMediaUrl().toStdString();

    MediaSoapWrapper soapWrapperGet(endpoint.c_str(), login, password);
    VideoConfigReq confRequest;
    confRequest.ConfigurationToken = confToken;
    VideoConfigResp confResponse;

    int soapRes = soapWrapperGet.getVideoEncoderConfiguration(confRequest, confResponse);
    if (soapRes != SOAP_OK || !confResponse.Configuration || !confResponse.Configuration->Resolution) {
        qWarning() << "QnPlSonyResource::updateResourceCapabilities: can't get video encoder (or received data is null) (token="
            << confToken.c_str() << ") from camera (URL: " << soapWrapperGet.getEndpointUrl() << ", UniqueId: " << getUniqueId()
            << "). GSoap error code: " << soapRes << ". " << soapWrapperGet.getLastError();
        return false;
    }

    typedef QSharedPointer<QList<ResolutionPair> > ResolutionListPtr;
    ResolutionListPtr resolutionListPtr(0);
    {
        QMutexLocker lock(&m_mutex);
        resolutionListPtr = ResolutionListPtr(new QList<ResolutionPair>(m_resolutionList)); //Sorted desc
    }

    MediaSoapWrapper soapWrapper(endpoint.c_str(), login, password);
    SetVideoConfigReq request;
    request.Configuration = confResponse.Configuration;
    request.ForcePersistence = false;
    SetVideoConfigResp response;

    int triesNumLeft = MAX_RESOLUTION_DECREASES_NUM;
    QList<ResolutionPair>::iterator it = resolutionListPtr->begin();

    for (; it != resolutionListPtr->end() && triesNumLeft > 0; --triesNumLeft)
    {
        request.Configuration->Resolution->Width = it->first;
        request.Configuration->Resolution->Height = it->second;

        soapRes = soapWrapper.setVideoEncoderConfiguration(request, response);
        if (soapRes != SOAP_OK) {
            if (soapWrapper.isConflictError()) {
                it = resolutionListPtr->erase(it);
                continue;
            }

            qWarning() << "QnPlSonyResource::updateResourceCapabilities: can't set video encoder options (token="
                << confToken.c_str() << ") from camera (URL: " << soapWrapper.getEndpointUrl() << ", UniqueId: " << getUniqueId()
                << "). GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();
            return false;
        }

        break;
    }

    if (triesNumLeft == MAX_RESOLUTION_DECREASES_NUM) {
        return true;
    }

    QMutexLocker lock(&m_mutex);
    m_resolutionList = *resolutionListPtr.data();
    return true;
}





