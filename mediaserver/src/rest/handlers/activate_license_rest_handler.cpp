#include <QUrlQuery>
#include <QUrl>
#include <QLocale>

#include "activate_license_rest_handler.h"
#include <utils/network/tcp_connection_priv.h>
#include <utils/common/util.h>
#include "nx_ec/data/api_runtime_data.h"
#include "utils/network/simple_http_client.h"
#include "common/common_module.h"
#include "licensing/license.h"
#include "api/runtime_info_manager.h"
#include "utils/license_usage_helper.h"
#include "api/app_server_connection.h"
#include "nx_ec/data/api_license_data.h"
#include "nx_ec/data/api_conversion_functions.h"
#include <utils/serialization/json_functions.h>
#include <utils/common/app_info.h>

static const int TCP_TIMEOUT = 1000 * 5;

CLHttpStatus QnActivateLicenseRestHandler::makeRequest(const QString& licenseKey, bool infoMode, QByteArray& response)
{
    // make check license request
    QUrl url(QN_LICENSE_URL);
    CLSimpleHTTPClient client(url.host(), url.port(80), TCP_TIMEOUT, QAuthenticator());

    ec2::ApiRuntimeData runtimeData = QnRuntimeInfoManager::instance()->items()->getItem(qnCommon->moduleGUID()).data;
    QUrlQuery params;
    params.addQueryItem(QLatin1String("license_key"), licenseKey);
    params.addQueryItem(QLatin1String("box"), runtimeData.box);
    params.addQueryItem(QLatin1String("brand"), runtimeData.brand);
    params.addQueryItem(QLatin1String("version"), QnAppInfo::engineVersion()); //TODO: #GDM replace with qnCommon->engineVersion()? And what if --override-version?

    QLocale locale;
    params.addQueryItem(QLatin1String("lang"), QLocale::languageToString(locale.language()));

    const QList<QByteArray> mainHardwareIds = qnLicensePool->mainHardwareIds();
    const QList<QByteArray> compatibleHardwareIds = qnLicensePool->compatibleHardwareIds();
    int hw = 0;
    for (const QByteArray& hwid: mainHardwareIds) {
        QString name;
        if (hw == 0)
            name = QLatin1String("oldhwid");
        else if (hw == 1)
            name = QLatin1String("hwid");
        else
            name = QString(QLatin1String("hwid%1")).arg(hw);

        params.addQueryItem(name, QLatin1String(hwid));

        hw++;
    }

    hw = 1;
    for(const QByteArray& hwid: compatibleHardwareIds) {
        QString name = QString(QLatin1String("chwid%1")).arg(hw);
        params.addQueryItem(name, QLatin1String(hwid));
        hw++;
    }

    if (infoMode)
        params.addQueryItem(lit("mode"), lit("info"));

    QByteArray request = params.query(QUrl::FullyEncoded).toUtf8();
    CLHttpStatus result = client.doPOST(url.path(), request);
    if (result == CL_HTTP_SUCCESS)
        client.readAll(response);
    return result;
}

int QnActivateLicenseRestHandler::executeGet(const QString &, const QnRequestParams & requestParams, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    ec2::ApiDetailedLicenseData reply;

    QString licenseKey = requestParams.value("key");
    if (licenseKey.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter, tr("Parameter 'key' is missed"));
        return CODE_OK;
    }
    if (licenseKey.length() != 19 || licenseKey.count("-") != 3) {
        result.setError(QnJsonRestResult::MissingParameter, tr("Invalid license serial number provided. Serial number MUST be in format AAAA-BBBB-CCCC-DDDD"));
        return CODE_OK;
    }

    QnLicensePtr license;
    for (int i = 0; i < 2; ++i) 
    {
        QByteArray response;
        bool isCheckMode = (i == 0);
        CLHttpStatus errCode = makeRequest(licenseKey, isCheckMode, response);
        if (errCode != CL_HTTP_SUCCESS || response.isEmpty())
        {
            result.setError(QnJsonRestResult::CantProcessRequest, tr("Network error has occurred during license activation. Error code: %1").arg(errCode));
            return CODE_OK;
        }
    
        QJsonObject errorMessage;
        if (QJson::deserialize(response, &errorMessage)) 
        {
            QString message = QnLicenseUsageHelper::activationMessage(errorMessage);
            result.setError(QnJsonRestResult::CantProcessRequest, tr("Can't activate license:  %1").arg(message));
            return CODE_OK;
        }

        QTextStream is(&response);
        is.setCodec("UTF-8");

        license = QnLicense::readFromStream(is);
        QnLicense::ErrorCode licenseErrCode;
        if (!license->isValid(&licenseErrCode, QnLicense::VM_CheckInfo)) 
        {
            result.setError(QnJsonRestResult::CantProcessRequest, tr("Can't activate license:  %1").arg(QnLicense::errorMessage(licenseErrCode)));
            return CODE_OK;
        }
    }    

    ec2::AbstractECConnectionPtr connect = QnAppServerConnectionFactory::getConnection2();
    QnLicenseList licenses;
    licenses << license;
    const ec2::ErrorCode errorCode = connect->getLicenseManager()->addLicensesSync(licenses);
    if( errorCode != ec2::ErrorCode::ok) {
        result.setError(QnJsonRestResult::CantProcessRequest, tr("Internal server error: %1").arg(ec2::toString(errorCode)));
        return CODE_OK;
    }
    fromResourceToApi(license, reply);
    result.setReply(reply);
    return CODE_OK;
}
