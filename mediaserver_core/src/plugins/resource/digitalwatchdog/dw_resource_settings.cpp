#ifdef ENABLE_ONVIF

#include "dw_resource_settings.h"

#include <core/resource/camera_advanced_param.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/network/http/http_types.h>
#include <nx/network/deprecated/asynchttpclient.h>

namespace {
    const QRegExp DW_RES_SETTINGS_FILTER(QLatin1String("[{},']"));
    const int kHttpReadTimeout = 1000 * 10;

    const int kPravisHttpPort = 10080;
    const int kNoError = 0;
    const int kParseError = -1;
}

DWAbstractCameraProxy::DWAbstractCameraProxy(const QString& host, int port, unsigned int timeout, const QAuthenticator& auth):
    m_host(host),
    m_port(port),
    m_timeout(timeout),
    m_auth(auth)
{}


// --------------- QnWin4NetCameraProxy ------------------
QnWin4NetCameraProxy::QnWin4NetCameraProxy(const QString& host, int port, unsigned int timeout, const QAuthenticator& auth):
    DWAbstractCameraProxy(host, port, timeout, auth)
{
}

QnCameraAdvancedParamValueList QnWin4NetCameraProxy::fetchParamsFromHttpResponse(const QByteArray& body) const {
    QnCameraAdvancedParamValueList result;

    //Fetching params sent in JSON format
    QList<QByteArray> lines = body.split(',');
    for (int i = 0; i < lines.size(); ++i) {
        QString str = QString::fromLatin1(lines[i]);
        str.replace(DW_RES_SETTINGS_FILTER, QString());
        QStringList pairStrs = str.split(L':');
        if (pairStrs.size() == 2)
        {
            auto paramId = pairStrs[0].trimmed();
            auto paramValue = pairStrs[1].trimmed();

            auto param = m_params.getParameterById(paramId);

            if (param.isValid())
                paramValue = fromInnerValue(param, paramValue);

            result << QnCameraAdvancedParamValue(paramId, paramValue);
        }
    }
    return result;
}

bool QnWin4NetCameraProxy::setParams(const QVector<QPair<QnCameraAdvancedParameter, QString>> &parameters, QnCameraAdvancedParamValueList *result)
{
    QnCameraAdvancedParamValueList tmpList;
    QString postMsgBody;
    for (const auto& data: parameters)
    {
        const QnCameraAdvancedParameter& parameter = data.first;
        const QString& value = data.second;
        if (parameter.tag == lit("POST")) {
            bool success = setParam(parameter, value); // we can't set such parameters together
            if (success) {
                if (result)
                    result->push_back(QnCameraAdvancedParamValue(parameter.id, value));
            }
            else {
                return false;
            }
        }
        else {
            QString innerValue = toInnerValue(parameter, value);
            if (!postMsgBody.isEmpty())
                postMsgBody.append(L'&');
            postMsgBody.append(parameter.id).append(L'=').append(innerValue);
            tmpList << QnCameraAdvancedParamValue(parameter.id, value);
        }
    }

    if (postMsgBody.isEmpty())
        return true;

    CLSimpleHTTPClient httpClient(m_host, m_port, m_timeout, m_auth);
    if (httpClient.doPOST(lit("cgi-bin/camerasetup.cgi"), postMsgBody) != CL_HTTP_SUCCESS)
        return false;
    if (result) {
        for (const auto& value: tmpList)
            result->push_back(value);
    }
    return true;
}

QString QnWin4NetCameraProxy::toInnerValue(const QnCameraAdvancedParameter &parameter, const QString &value) const
{
    QString innerValue = value;
    if (parameter.dataType == QnCameraAdvancedParameter::DataType::Enumeration) {
        int idx = parameter.getRange().indexOf(value);
        if (idx < 0)
            return QString();
        innerValue = QString::number(idx);
    } else if (parameter.dataType == QnCameraAdvancedParameter::DataType::Bool) {
        int idx = value == lit("true") ? 1 : 0;
        innerValue = QString::number(idx);
    }

    return innerValue;
}

QString QnWin4NetCameraProxy::fromInnerValue(const QnCameraAdvancedParameter& parameter, const QString& value) const
{
    bool ok = true;
    std::size_t idx = value.toUInt(&ok);

    if (!ok)
        return QString();

    if (parameter.dataType == QnCameraAdvancedParameter::DataType::Enumeration)
    {
        auto range = parameter.getRange();

        if (range.size() <= idx)
            return QString();

        return range[idx];
    }
    else if (parameter.dataType == QnCameraAdvancedParameter::DataType::Bool)
    {
        return value == lit("0") ? lit("false") : lit("true");
    }

    return value;
}

bool QnWin4NetCameraProxy::setParam(const QnCameraAdvancedParameter &parameter, const QString &value)
{
    QString innerValue = toInnerValue(parameter, value);

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

QnCameraAdvancedParamValueList QnWin4NetCameraProxy::requestParamValues(const QString &request) const {
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

QnCameraAdvancedParamValueList QnWin4NetCameraProxy::getParamsList() const {
    QnCameraAdvancedParamValueList result;
    result.append(requestParamValues(lit("cgi-bin/getconfig.cgi?action=color")));
    result.append(requestParamValues(lit("cgi-bin/getconfig.cgi?action=ftpUpgradeInfo")));
    return result;
}

// -------------------- QnPravisCameraProxy --------------------

QnPravisCameraProxy::QnPravisCameraProxy(const QString& host, int port, unsigned int timeout, const QAuthenticator& auth):
    DWAbstractCameraProxy(host, port, timeout, auth)
{
}

void QnPravisCameraProxy::setCameraAdvancedParams(const QnCameraAdvancedParams &params)
{
    // obtain plain list of the parameters here
    for (const auto& group: params.groups)
        addToFlatParams(group);
}

void QnPravisCameraProxy::addToFlatParams(const QnCameraAdvancedParamGroup& group)
{
    for (const auto& subGroup: group.groups)
        addToFlatParams(subGroup);
    for (const auto& param: group.params)
        m_flatParams.insert(param.id, param);
}

QString parseParamFromHttpResponse(const QnCameraAdvancedParameter& cameraAdvParam, nx::network::http::BufferType msgBody)
{
    // camera returns result as human readable page. The page format depends on parameter name.
    // There are two different formats so far. Param value follows after the prefix
    const QByteArray pattern1 = (cameraAdvParam.id != "wdrmode" ? "byte(" : "camfw_setting_param");

    for (auto line: msgBody.split('\n'))
    {
        line = line.trimmed();

        int pattern1Pos = line.indexOf(pattern1);
        if (pattern1Pos == -1)
            continue;
        int eqPos = line.indexOf(L'=', pattern1Pos);
        if (eqPos == -1)
            continue;
        QByteArray value = line.mid(eqPos + 1).trimmed();
        value = value.split(' ')[0];
        if (value.startsWith("0x"))
            value = QByteArray::number(value.toInt(0, 16)); // convert result from HEX value
        return QLatin1String(value);
    }
    return QString();
}

QnCameraAdvancedParamValueList QnPravisCameraProxy::getParamsList() const
{
    QnCameraAdvancedParamValueList result;
    int workers = 0;

    QnMutex waitMutex;
    QnWaitCondition waitCond;

    QnMutexLocker lock(&waitMutex);
    for (const auto& cameraAdvParam: m_flatParams)
    {
        if (cameraAdvParam.dataType == QnCameraAdvancedParameter::DataType::Button) {
            result << QnCameraAdvancedParamValue(cameraAdvParam.id, QString()); // just inform param exists
            continue;
        }

        nx::utils::Url apiUrl = lit("http://%1:%2/cgi-bin/ne3exec.cgi/%3").arg(m_host).arg(kPravisHttpPort).arg(cameraAdvParam.readCmd);
        apiUrl.setUserName(m_auth.user());
        apiUrl.setPassword(m_auth.password());

        auto requestCompletionFunc = [&](SystemError::ErrorCode, int statusCode, nx::network::http::StringType, nx::network::http::BufferType msgBody)
        {
            {
                QnMutexLocker lock(&waitMutex);
                QnCameraAdvancedParamValue param;
                if (statusCode == nx::network::http::StatusCode::ok && !msgBody.isEmpty())
                    param.value = fromInnerValue(cameraAdvParam, parseParamFromHttpResponse(cameraAdvParam, msgBody));
                else
                    qWarning() << "error reading param" << cameraAdvParam.id << "for camera" << m_host;
                param.id = cameraAdvParam.id;
                result << param;
                --workers;
            }
            waitCond.wakeAll();
        };

        nx::network::http::AsyncHttpClient::Timeouts timeouts;
        timeouts.responseReadTimeout = std::chrono::milliseconds(kHttpReadTimeout);
        nx::network::http::downloadFileAsyncEx(
            apiUrl, requestCompletionFunc, nx::network::http::HttpHeaders(),
            nx::network::http::AuthType::authBasicAndDigest, std::move(timeouts));
        ++workers;
    }
    while (workers > 0)
        waitCond.wait(&waitMutex);

    return result;
}


QString QnPravisCameraProxy::toInnerValue(const QnCameraAdvancedParameter &parameter, const QString &value) const
{
    if (parameter.dataType == QnCameraAdvancedParameter::DataType::Enumeration)
        return parameter.toInternalRange(value);
    else
        return value;
}

QString QnPravisCameraProxy::fromInnerValue(const QnCameraAdvancedParameter &parameter, const QString &value) const
{
    if (parameter.dataType == QnCameraAdvancedParameter::DataType::Enumeration)
        return parameter.fromInternalRange(value);
    else
        return value;
}

int httpResultErrCode(const QByteArray& msgBody)
{
    int idx1 = msgBody.indexOf("<status>");
    if (idx1 == -1)
        return kParseError;
    idx1 += QByteArray("<status>").length();

    int idx2 = msgBody.indexOf("</status>");
    if (idx2 == -1)
        return kParseError;

    return msgBody.mid(idx1, idx2 - idx1).toInt();
}

bool QnPravisCameraProxy::setParams(const QVector<QPair<QnCameraAdvancedParameter, QString>> &parameters, QnCameraAdvancedParamValueList *result)
{
    int workers = 0;

    QnMutex waitMutex;
    QnWaitCondition waitCond;
    bool resultOK = true;

    QnMutexLocker lock(&waitMutex);
    for (const auto& pair: parameters)
    {
        const auto& parameter = pair.first;
        const auto& value = pair.second;

        QnCameraAdvancedParameter cameraAdvParam = m_flatParams.value(parameter.id);
        if (cameraAdvParam.id.isNull())
            continue; //< param not found. skip

        QString command = cameraAdvParam.writeCmd;
        command = command.replace("$", toInnerValue(cameraAdvParam, value));
        QString path = lit("cgi-bin/ne3exec.cgi/%1").arg(command);

        nx::utils::Url apiUrl = lit("http://%1:%2/%3").arg(m_host).arg(kPravisHttpPort).arg(path);
        apiUrl.setUserName(m_auth.user());
        apiUrl.setPassword(m_auth.password());

        auto requestCompletionFunc = [&](SystemError::ErrorCode, int statusCode, nx::network::http::StringType, nx::network::http::BufferType msgBody)
        {
            {
                QnMutexLocker lock(&waitMutex);
                if (statusCode == nx::network::http::StatusCode::ok &&  httpResultErrCode(msgBody) == kNoError)
                {
                    QnCameraAdvancedParamValue param;
                    param.value = value;
                    param.id = cameraAdvParam.id;
                    if (result)
                        result->push_back(param);
                }
                else {
                    resultOK = false;
                }
                --workers;
            }
            waitCond.wakeAll();
        };

        nx::network::http::AsyncHttpClient::Timeouts timeouts;
        timeouts.responseReadTimeout = std::chrono::milliseconds(kHttpReadTimeout);
        nx::network::http::downloadFileAsyncEx(
            apiUrl, requestCompletionFunc, nx::network::http::HttpHeaders(),
            nx::network::http::AuthType::authBasicAndDigest, std::move(timeouts));
        ++workers;
    }
    while (workers > 0)
        waitCond.wait(&waitMutex);

    return resultOK;
}

#endif  // ENABLE_ONVIF
