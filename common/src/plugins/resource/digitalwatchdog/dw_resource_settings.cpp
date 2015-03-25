#include "dw_resource_settings.h"

#include <core/resource/camera_advanced_param.h>

#include <utils/common/model_functions.h>
#include <utils/network/simple_http_client.h>

namespace {
    const QRegExp DW_RES_SETTINGS_FILTER(lit("[{},']"));
}

//
// class DWCameraProxy
//
DWCameraProxy::DWCameraProxy(const QString& host, int port, unsigned int timeout, const QAuthenticator& auth):
    m_host(host),
    m_port(port),
    m_timeout(timeout),
    m_auth(auth)
{}

QnCameraAdvancedParamValueList DWCameraProxy::fetchParamsFromHttpResponse(const QByteArray& body) const {
    QnCameraAdvancedParamValueList result;

    //Fetching params sent in JSON format
    QList<QByteArray> lines = body.split(',');
    for (int i = 0; i < lines.size(); ++i) {
        QString str = QString::fromLatin1(lines[i]);
        str.replace(DW_RES_SETTINGS_FILTER, QString());
        QStringList pairStrs = str.split(L':');
        if (pairStrs.size() == 2) {
            result << QnCameraAdvancedParamValue(pairStrs[0].trimmed(), pairStrs[1].trimmed());
        }
    }
    return result;
}


bool DWCameraProxy::setParam(const QnCameraAdvancedParameter &parameter, const QString &value) {

    QString innerValue = value;
    if (parameter.dataType == QnCameraAdvancedParameter::DataType::Enumeration) {
        int idx = parameter.getRange().indexOf(value);
        if (idx < 0)
            return false;
        innerValue = QString::number(idx);
    } else if (parameter.dataType == QnCameraAdvancedParameter::DataType::Bool) {
        int idx = value == lit("true") ? 1 : 0;
        innerValue = QString::number(idx);
    }

    CLSimpleHTTPClient httpClient(m_host, m_port, m_timeout, m_auth);

    if (parameter.tag == lit("POST")) {     
        QString paramQuery;
        QString query;

        if (parameter.id.startsWith(QLatin1String("/"))) {
            query = parameter.id;
            paramQuery = lit("action=all");
        } else {
            query = lit("/cgi-bin/systemsetup.cgi");
            paramQuery = lit("ftp_upgrade_") + paramQuery + lit("=") + innerValue;
        }

        return httpClient.doPOST(query, paramQuery) == CL_HTTP_SUCCESS;
    } else {
        QString paramQuery = parameter.id;
        if (!parameter.id.startsWith(L'/'))
            paramQuery = lit("cgi-bin/camerasetup.cgi?") + parameter.id;
        if (!value.isEmpty())
            paramQuery = paramQuery + L'=' + innerValue;
        return httpClient.doGET(paramQuery) == CL_HTTP_SUCCESS;
    }
}


QnCameraAdvancedParamValueList DWCameraProxy::requestParamValues(const QString &request) const {
    CLSimpleHTTPClient httpClient(m_host, m_port, m_timeout, m_auth);
    CLHttpStatus status = httpClient.doGET(request);
    if (status == CL_HTTP_SUCCESS) 
    {
        QByteArray body;
        httpClient.readAll(body);
        return fetchParamsFromHttpResponse(body);;
    }
    qWarning() << "DWCameraProxy::getFromCameraImpl: HTTP GET request '" << request << "' failed: status: " << status << "host=" << m_host << ":" << m_port;
    return QnCameraAdvancedParamValueList();
}

QnCameraAdvancedParamValueList DWCameraProxy::getParamsList() const {
    QnCameraAdvancedParamValueList result;
    result.append(requestParamValues(lit("cgi-bin/getconfig.cgi?action=color")));
    result.append(requestParamValues(lit("cgi-bin/getconfig.cgi?action=ftpUpgradeInfo")));  
    return result;
}


/*

void DWCameraSetting::initAdditionalValues()
{
    if ( (getType() != CameraSetting::Enumeration && getType() != CameraSetting::OnOff) ||
        !m_enumStrToInt.isEmpty())
    {
        return;
    }

    if (getType() == CameraSetting::Enumeration)
    {
        m_enumStrToInt = static_cast<QString>(getMin()).split(QLatin1String(","));
        return;
    }

    if (getType() == CameraSetting::OnOff)
    {
        QString stepStr = getStep();
        //Step is required, because not always Off = 0 and On = 1
        unsigned int step = stepStr.isEmpty()? 1 : stepStr.toUInt();

        while (step-- > 0) {
            m_enumStrToInt.push_back(getMin());
        }
        m_enumStrToInt.push_back(getMax());

        return;
    }
}*/
