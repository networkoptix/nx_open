#include <QUrlQuery>
#include <QUrl>
#include <QLocale>
#include <QtCore/QFile>

#include "activate_license_rest_handler.h"
#include <network/tcp_connection_priv.h>
#include <utils/common/util.h>
#include <nx/network/deprecated/simple_http_client.h>
#include "common/common_module.h"
#include "licensing/license.h"
#include "api/runtime_info_manager.h"
#include "utils/license_usage_helper.h"
#include "api/app_server_connection.h"
#include <nx_ec/ec_api.h>
#include "nx_ec/data/api_conversion_functions.h"
#include <nx/fusion/serialization/json_functions.h>
#include <utils/common/app_info.h>
#include "rest/server/rest_connection_processor.h"
#include <llutil/hardware_id.h>
#include <licensing/license_validator.h>
#include <licensing/hardware_id_version.h>
#include <nx/vms/api/data/license_data.h>

static const int TCP_TIMEOUT = 1000 * 5;

#ifdef Q_OS_LINUX
#include <nx/vms/server/system/nx1/info.h>
#endif

using namespace nx;

namespace {

const QString kLicenseKey = "license_key";
const QString kBox = "box";
const QString kBrand = "brand";
const QString kVersion = "version";
const QString kMac = "mac";
const QString kSerial = "serial";
const QString kLang = "lang";
const QString kOldHwidArray = "oldhwid[]";
const QString kHwid = "hwid";
const QString kHwidArray = "hwid[]";
const QString kMode = "mode";
const QString kInfo = "info";
const QString kKey = "key";

// TODO: Some strings can be reused when when the server supports translations.
QString activationMessage(const QJsonObject& errorMessage)
{
    QString messageId = errorMessage.value("messageId").toString();

    // TODO: Feature #3629 case J
    if (messageId == "DatabaseError")
        return "There was a problem activating your License Key. A database error occurred.";

    if (messageId == "InvalidData")
    {
        return "There was a problem activating your License Key. Invalid data received. "
            "Please contact the support team to report the issue.";
    }

    if (messageId == "InvalidKey")
    {
        return "License Key you have entered is invalid. Please check that License Key is "
            "entered correctly. If problem persists, please contact the support team to confirm "
            "if License Key is valid or to obtain a valid License Key.";
    }

    if (messageId == "InvalidBrand")
    {
        return "You are trying to activate a license incompatible with your software. Please "
            "contact the support team to obtain a valid License Key.";
    }

    if (messageId == "AlreadyActivated")
    {
        QString message =
            "This License Key has been previously activated to Hardware Id %1 on %2. "
            "Please contact the support team to obtain a valid License Key.";

        QVariantMap arguments = errorMessage.value("arguments").toObject().toVariantMap();
        QString hwid = arguments.value("hwid").toString();
        QString time = arguments.value("time").toString();

        return message.arg(hwid).arg(time);
    }

    return errorMessage.value("message").toString();
}

}

struct LicenseKey
{
    QString licenseKey;
};
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((LicenseKey), (json))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LicenseKey, (json), (licenseKey))

int QnActivateLicenseRestHandler::executePost(const QString& /*path*/,
    const QnRequestParams& /*params*/, const QByteArray& body, QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    LicenseKey licenseKeyStruct;
    if (!QJson::deserialize<LicenseKey>(body, &licenseKeyStruct))
    {
        result.setError(QnJsonRestResult::InvalidParameter, "Invalid content");
        return nx::network::http::StatusCode::ok;
    }

    return activateLicense(licenseKeyStruct.licenseKey, result, owner);
}

// WARNING: Deprecated!
int QnActivateLicenseRestHandler::executeGet(const QString& /*path*/, const QnRequestParams& params,
    QnJsonRestResult& result, const QnRestConnectionProcessor* owner)
{
    const QString licenseKey = params.value(kKey);
    if (licenseKey.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, "Parameter 'key' is missed");
        return nx::network::http::StatusCode::ok;
    }

    return activateLicense(licenseKey, result, owner);
}

int QnActivateLicenseRestHandler::activateLicense(const QString& licenseKey,
    QnJsonRestResult& result, const QnRestConnectionProcessor* owner)
{
    vms::api::DetailedLicenseData reply;
    if (licenseKey.length() != 19 || licenseKey.count("-") != 3)
    {
        result.setError(QnJsonRestResult::InvalidParameter,
            "Invalid license serial number provided. "
            "Serial number MUST be in format AAAA-BBBB-CCCC-DDDD");
        return nx::network::http::StatusCode::ok;
    }

    QnLicensePtr license;
    for (int i = 0; i < 2; ++i)
    {
        QByteArray response;
        bool isCheckMode = (i == 0);
        CLHttpStatus errCode = makeRequest(owner->commonModule(), licenseKey, isCheckMode, response);
        if (errCode != CL_HTTP_SUCCESS || response.isEmpty())
        {
            result.setError(QnJsonRestResult::CantProcessRequest,
                QString("Network error has occurred during license activation. "
                    "Error code: %1").arg(errCode));
            return nx::network::http::StatusCode::ok;
        }

        QJsonObject errorMessage;
        if (QJson::deserialize(response, &errorMessage))
        {
            QString message = activationMessage(errorMessage);
            result.setError(QnJsonRestResult::CantProcessRequest,
                QString("Can't activate license:  %1").arg(message));
            return nx::network::http::StatusCode::ok;
        }

        QTextStream is(&response);
        is.setCodec("UTF-8");

        license = QnLicense::readFromStream(is);
        QnLicenseValidator validator(owner->commonModule());
        auto licenseErrCode = validator.validate(license, QnLicenseValidator::VM_CanActivate);
        if (licenseErrCode != QnLicenseErrorCode::NoError)
        {
            result.setError(
                QnJsonRestResult::CantProcessRequest,
                QString("Can't activate license: %1").arg(
                    QnLicenseValidator::errorMessage(licenseErrCode)));
            return nx::network::http::StatusCode::ok;
        }
    }

    ec2::AbstractECConnectionPtr connect = owner->commonModule()->ec2Connection();
    QnLicenseList licenses;
    licenses << license;
    auto licenseManager = connect->makeLicenseManager(owner->accessRights());

    const ec2::ErrorCode errorCode = licenseManager->addLicensesSync(licenses);
    if (errorCode == ec2::ErrorCode::forbidden)
    {
        result.setError(QnJsonRestResult::Forbidden,
            QString("Can't activate license because user has not enough access rights."));
        return nx::network::http::StatusCode::ok;
    }

    if (errorCode != ec2::ErrorCode::ok)
    {
        result.setError(QnJsonRestResult::CantProcessRequest,
            QString("Internal server error: %1").arg(ec2::toString(errorCode)));
        return nx::network::http::StatusCode::ok;
    }
    ec2::fromResourceToApi(license, reply);
    result.setReply(reply);
    return nx::network::http::StatusCode::ok;
}

CLHttpStatus QnActivateLicenseRestHandler::makeRequest(
    QnCommonModule* commonModule,
    const QString& licenseKey,
    bool infoMode,
    QByteArray& response)
{
    const auto url = QnLicenseServer::kActivateUrl;
    CLSimpleHTTPClient client(url.host(), url.port(80), TCP_TIMEOUT, QAuthenticator());

    const auto runtimeData = commonModule->runtimeInfoManager()->items()
        ->getItem(commonModule->moduleGUID()).data;

    QUrlQuery params;
    params.addQueryItem(kLicenseKey, licenseKey);
    params.addQueryItem(kBox, runtimeData.box);
    params.addQueryItem(kBrand, runtimeData.brand);
    // TODO: #GDM replace with qnStaticCommon->engineVersion()? And what if --override-version?
    params.addQueryItem(kVersion, QnAppInfo::engineVersion());

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

    const QVector<QString> hardwareIds = commonModule->licensePool()->hardwareIds();
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
