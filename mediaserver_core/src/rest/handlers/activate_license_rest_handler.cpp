#include <QUrlQuery>
#include <QUrl>
#include <QLocale>
#include <QtCore/QFile>

#include "activate_license_rest_handler.h"
#include <network/tcp_connection_priv.h>
#include <utils/common/util.h>
#include "nx_ec/data/api_runtime_data.h"
#include <nx/network/simple_http_client.h>
#include "common/common_module.h"
#include "licensing/license.h"
#include "api/runtime_info_manager.h"
#include "utils/license_usage_helper.h"
#include "api/app_server_connection.h"
#include "nx_ec/data/api_license_data.h"
#include "nx_ec/data/api_conversion_functions.h"
#include <nx/fusion/serialization/json_functions.h>
#include <utils/common/app_info.h>
#include "rest/server/rest_connection_processor.h"
#include <llutil/hardware_id.h>

static const int TCP_TIMEOUT = 1000 * 5;

#ifdef Q_OS_LINUX
#include "nx1/info.h"
#endif

namespace
{
    const QString kLicenseKey = lit("license_key");
    const QString kBox = lit("box");
    const QString kBrand = lit("brand");
    const QString kVersion = lit("version");
    const QString kMac = lit("mac");
    const QString kSerial = lit("serial");
    const QString kLang = lit("lang");
    const QString kOldHwidArray = lit("oldhwid[]");
    const QString kHwid = lit("hwid");
    const QString kHwidArray = lit("hwid[]");
    const QString kMode = lit("mode");
    const QString kInfo = lit("info");
    const QString kKey = lit("key");
}

CLHttpStatus QnActivateLicenseRestHandler::makeRequest(const QString& licenseKey, bool infoMode, QByteArray& response)
{
    // make check license request
    QUrl url(QN_LICENSE_URL);
    CLSimpleHTTPClient client(url.host(), url.port(80), TCP_TIMEOUT, QAuthenticator());

    ec2::ApiRuntimeData runtimeData = runtimeInfoManager()->items()->getItem(commonModule()->moduleGUID()).data;
    QUrlQuery params;
    params.addQueryItem(kLicenseKey, licenseKey);
    params.addQueryItem(kBox, runtimeData.box);
    params.addQueryItem(kBrand, runtimeData.brand);
    params.addQueryItem(kVersion, QnAppInfo::engineVersion()); //TODO: #GDM replace with commonModule()->engineVersion()? And what if --override-version?

#ifdef Q_OS_LINUX
    if(QnAppInfo::isBpi() || QnAppInfo::isNx1())
    {
        QString mac = Nx1::getMac();
        QString serial = Nx1::getSerial();

        if (!mac.isEmpty())
            params.addQueryItem(kMac, mac);

        if (!serial.isEmpty())
            params.addQueryItem(kSerial, serial);
    }
#endif

    QLocale locale;
    params.addQueryItem(kLang, QLocale::languageToString(locale.language()));

    const QVector<QString> hardwareIds = licensePool()->hardwareIds();
    for (const QString& hwid: hardwareIds)
    {
        int version = LLUtil::hardwareIdVersion(hwid);

        QString name;
        if (version == 0)
            name = kOldHwidArray;
        else if (version == 1)
            name = kHwidArray;
        else
            name = QString(QLatin1String("%1%2[]")).arg(kHwid).arg(version);

        params.addQueryItem(name, hwid);
    }

    if (infoMode)
        params.addQueryItem(kMode, kInfo);

    QByteArray request = params.query(QUrl::FullyEncoded).toUtf8();
    CLHttpStatus result = client.doPOST(url.path(), request);
    if (result == CL_HTTP_SUCCESS)
        client.readAll(response);
    return result;
}

int QnActivateLicenseRestHandler::executeGet(const QString &, const QnRequestParams & requestParams, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    ec2::ApiDetailedLicenseData reply;

    QString licenseKey = requestParams.value(kKey);
    if (licenseKey.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, lit("Parameter 'key' is missed"));
        return CODE_OK;
    }
    if (licenseKey.length() != 19 || licenseKey.count("-") != 3)
    {
        result.setError(QnJsonRestResult::MissingParameter, lit("Invalid license serial number provided. Serial number MUST be in format AAAA-BBBB-CCCC-DDDD"));
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
            result.setError(QnJsonRestResult::CantProcessRequest, lit("Network error has occurred during license activation. Error code: %1").arg(errCode));
            return CODE_OK;
        }

        QJsonObject errorMessage;
        if (QJson::deserialize(response, &errorMessage))
        {
            QString message = QnLicenseUsageHelper::activationMessage(errorMessage);
            result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't activate license:  %1").arg(message));
            return CODE_OK;
        }

        QTextStream is(&response);
        is.setCodec("UTF-8");

        license = QnLicense::readFromStream(is);
        QnLicense::ErrorCode licenseErrCode;
        if (!license->isValid(&licenseErrCode, QnLicense::VM_CheckInfo))
        {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't activate license:  %1").arg(QnLicense::errorMessage(licenseErrCode)));
            return CODE_OK;
        }
    }

    ec2::AbstractECConnectionPtr connect = commonModule()->ec2Connection();
    QnLicenseList licenses;
    licenses << license;
    auto licenseManager = connect->getLicenseManager(owner->accessRights());
    const ec2::ErrorCode errorCode = licenseManager->addLicensesSync(licenses);
    NX_ASSERT(errorCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
    if( errorCode != ec2::ErrorCode::ok)
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error: %1").arg(ec2::toString(errorCode)));
        return CODE_OK;
    }
    fromResourceToApi(license, reply);
    result.setReply(reply);
    return CODE_OK;
}
