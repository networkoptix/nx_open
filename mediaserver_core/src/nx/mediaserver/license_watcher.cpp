#include "license_watcher.h"

#include <common/common_module.h>
#include <nx_ec/dummy_handler.h>
#include "ec2_connection.h"
#include <nx/fusion/serialization/json.h>
#include <llutil/hardware_id.h>
#include <nx/utils/app_info.h>
#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/fusion/serialization_format.h>
#include <nx/network/http/buffer_source.h>
#include <nx/utils/log/log_main.h>

namespace nx {
namespace mediaserver {

static const int kCheckIntervalMs = 1000 * 3600 * 24; //< Once a day.

struct ServerInfo
{
    QString brand;
    QString version;
    QString latestHwid;
    QStringList hwids;
};

struct ServerLicenseInfo
{
    ServerInfo info;
    ec2::ApiDetailedLicenseDataList licenses;
};

struct LicenseError
{
    QString code;
    QString text;
};

struct CheckLicenseResponse
{
    QString status;
    QMap<QString, LicenseError> licenseErrors;
    QVector<ec2::ApiDetailedLicenseData> licenses;
};

QN_FUSION_ADAPT_STRUCT(ServerInfo, (brand)(version)(latestHwid)(hwids))
QN_FUSION_ADAPT_STRUCT(ServerLicenseInfo, (info)(licenses))
QN_FUSION_ADAPT_STRUCT(LicenseError, (code)(text))
QN_FUSION_ADAPT_STRUCT(CheckLicenseResponse, (status)(licenseErrors)(licenses))

QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(
    (ServerInfo)(ServerLicenseInfo)(LicenseError)(CheckLicenseResponse),
    (json))

LicenseWatcher::LicenseWatcher(QnCommonModule* commonModule) :
    base_type(commonModule)
{
    connect(&m_timer, &QTimer::timeout, this, &LicenseWatcher::update);
}

void LicenseWatcher::start()
{
    m_httpClient.reset(new nx_http::AsyncClient());
    update();
    m_timer.start(kCheckIntervalMs);
}

ServerLicenseInfo LicenseWatcher::licenseData() const
{
    ec2::ApiRuntimeData runtimeData = runtimeInfoManager()->items()->getItem(commonModule()->moduleGUID()).data;

    ServerLicenseInfo result;
    result.info.brand = runtimeData.brand;
    result.info.version = nx::utils::AppInfo::applicationVersion();
    result.info.latestHwid = LLUtil::getLatestHardwareId();
    result.info.hwids = LLUtil::getAllHardwareIds();

    auto connectionPtr = commonModule()->ec2Connection();
    if (!connectionPtr)
        return result;
    QnLicenseList licenseList;
    auto licenseManager = connectionPtr->getLicenseManager(Qn::kSystemAccess);
    if (licenseManager->getLicensesSync(&licenseList) != ec2::ErrorCode::ok)
        return result;
    for (const QnLicensePtr& license: licenseList)
    {
        ec2::ApiDetailedLicenseData licenseData;
        ec2::fromResourceToApi(license, licenseData);
        result.licenses.push_back(licenseData);
    }

    return result;
}

void LicenseWatcher::update()
{
    static const QUrl kLicenseServerUrl("http://nxlicensed.hdw.mx/nxlicensed/api/v1/validate/");

    auto requestBody = QJson::serialized(licenseData());
    m_httpClient->setRequestBody(
        std::make_unique<nx_http::BufferSource>(
            serializationFormatToHttpContentType(Qn::JsonFormat),
            std::move(requestBody)));

    m_httpClient->doPost(kLicenseServerUrl, std::bind(&LicenseWatcher::onHttpClientDone, this));
}

void LicenseWatcher::onHttpClientDone()
{
    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return; //< Serer is about to stop

    nx_http::AsyncClient::State state = m_httpClient->state();
    if (state == nx_http::AsyncClient::sFailed)
    {
        NX_WARNING(
            this,
            lm("Can't update license list. Http request to the license server failed with error %1")
            .arg(m_httpClient->lastSysErrorCode()));
        return;
    }

    const int statusCode = m_httpClient->response()->statusLine.statusCode;
    if (statusCode != nx_http::StatusCode::ok)
    {
        NX_WARNING(this, lm("Can't read response from the license server. Http error code %1").arg(statusCode));
        return;
    }

    QByteArray response = m_httpClient->fetchMessageBodyBuffer();

    bool success = false;
    auto newLicenses = QJson::deserialized<CheckLicenseResponse>(response, CheckLicenseResponse(), &success);
    if (!success)
    {
        NX_WARNING(this, lm("Can't deserialize response from the license server."
            "Not a correct JSON object is received."));
        return;
    }

    auto licenseManager = connection->getLicenseManager(Qn::kSystemAccess);

    for (auto itr = newLicenses.licenseErrors.begin();
        itr != newLicenses.licenseErrors.end();
        ++itr)
    {
        QByteArray licenseKey = itr.key().toUtf8();
        auto errorInfo = itr.value();
        NX_INFO(this, lm("License %1 has been removed. Reason: '%2'. Error code: %3")
            .arg(licenseKey).arg(errorInfo.text).arg(errorInfo.code));

        QnLicensePtr licenseObject(QnLicense::createFromKey(licenseKey));
        auto dbErrorCode = licenseManager->removeLicenseSync(licenseObject);
        if (dbErrorCode != ec2::ErrorCode::ok)
        {
            NX_WARNING(
                this, 
                lm("Can't remove license key %1 from the database. DB eror code %2")
                .arg(licenseKey)
                .arg(ec2::toString(dbErrorCode)));
        }
    }

    QList<QnLicensePtr> licenses;
    for (const auto& licenseData: newLicenses.licenses)
        licenses << QnLicensePtr(new QnLicense(licenseData));
    auto dbErrorCode = licenseManager->addLicensesSync(licenses);
    if (dbErrorCode != ec2::ErrorCode::ok)
    {
        NX_WARNING(
            this, 
            lm("Can't update licenses into the database. DB error %1")
            .arg(ec2::toString(dbErrorCode)));
    }
}

} // namespace mediaserver
} // namespace nx

