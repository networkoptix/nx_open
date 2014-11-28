#ifndef dw_resource_settings_h_1957
#define dw_resource_settings_h_1957

#include <QtNetwork/QAuthenticator>

#include <core/resource/camera_advanced_param.h>

//
// class DWCameraProxy
//
class DWCameraProxy
{
    const QString m_host;
    int m_port;
    unsigned int m_timeout;
    const QAuthenticator m_auth;
public:
    DWCameraProxy(const QString& host, int port, unsigned int timeout, const QAuthenticator& auth);

    QnCameraAdvancedParamValueList getParamsList() const;
    bool setParam(const QString &id, const QString &value, const QString &method);
private:
    QnCameraAdvancedParamValueList fetchParamsFromHttpResponse(const QByteArray& body) const;
    QnCameraAdvancedParamValueList requestParamValues(const QString &request) const;
private:
    QnCameraAdvancedParamValueMap m_valuesCache;
    QnCameraAdvancedParams m_params;
};


#endif //dw_resource_settings_h_1957
