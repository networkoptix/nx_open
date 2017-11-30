#include "license_watcher.h"

#include <chrono>

#include "ec2_connection.h"

#include <common/common_module.h>
#include <llutil/hardware_id.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/dummy_handler.h>
#include <nx_ec/ec_api.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/buffer_source.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/scope_guard.h>
#include <utils/common/delayed.h>
#include <core/resource/media_server_resource.h>

namespace nx {
namespace mediaserver {

static const nx::utils::Url kLicenseServerUrl("http://licensing.networkoptix.com/nxlicensed/api/v1/validate/");
static const std::chrono::milliseconds kCheckInterval = std::chrono::hours(24);

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

LicenseWatcher::LicenseWatcher(QnCommonModule* commonModule):
    base_type(commonModule)
{
    connect(&m_timer, &QTimer::timeout, this, &LicenseWatcher::startUpdate);
}

LicenseWatcher::~LicenseWatcher()
{
    stopHttpClient();
}

void LicenseWatcher::start()
{
    startUpdate();
    m_timer.start(kCheckInterval.count());
}

void LicenseWatcher::startUpdate()
{
    auto data = licenseData();
    if (data.licenses.empty())
        return; //< Nothing to update.

    stopHttpClient();
    m_httpClient = std::make_unique<nx_http::AsyncClient>();

    auto server = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
    if (!server)
        return;

    m_httpClient->setProxyVia(
    SocketAddress(HostAddress::localhost, QUrl(server->getUrl()).port()));
    m_httpClient->setProxyUserName(server->getId().toString());
    m_httpClient->setProxyUserPassword(server->getAuthKey());

    auto requestBody = QJson::serialized(data);
    m_httpClient->setRequestBody(
        std::make_unique<nx_http::BufferSource>(
            serializationFormatToHttpContentType(Qn::JsonFormat),
            std::move(requestBody)));

    QPointer<QObject> objectGuard(this);
    m_httpClient->doPost(kLicenseServerUrl,
        [this, objectGuard]()
        {
            NX_ASSERT(objectGuard, "Destructor must wait for m_httpClient stop.");
            const auto scopeGuard = makeScopeGuard([this](){ stopHttpClient(); });

            nx_http::AsyncClient::State state = m_httpClient->state();
            if (state == nx_http::AsyncClient::State::sFailed)
            {
                NX_WARNING(this,
                    lm("Can't update license list. HTTP request to the license server failed: %1")
                        .arg(SystemError::toString(m_httpClient->lastSysErrorCode())));
                return;
            }

            const int statusCode = m_httpClient->response()->statusLine.statusCode;
            if (statusCode != nx_http::StatusCode::ok)
            {
                NX_WARNING(this, lm("Can't read response from the license server. Http error code %1")
                    .arg(statusCode));
                return;
            }

            auto response = m_httpClient->fetchMessageBodyBuffer();
            executeInThread(objectGuard->thread(),
                [this, objectGuard, response = std::move(response)]() mutable
                {
                    if (objectGuard)
                        processResponse(std::move(response));
                });
        });
}

void LicenseWatcher::processResponse(QByteArray responseData)
{
    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return; //< Server is about to stop.

    CheckLicenseResponse response;
    if (!QJson::deserialize(responseData, &response))
    {
        NX_WARNING(this, lm("Can't deserialize response from the license server."));
        return;
    }

    auto licenseManager = connection->getLicenseManager(Qn::kSystemAccess);
    for (auto itr = response.licenseErrors.begin(); itr != response.licenseErrors.end(); ++itr)
    {
        QByteArray licenseKey = itr.key().toUtf8();
        auto errorInfo = itr.value();
        NX_ALWAYS(this, lm("License %1 has been removed. Reason: '%2'. Error code: %3")
            .args(licenseKey, errorInfo.text, errorInfo.code));

        auto licenseObject = QnLicense::createFromKey(licenseKey);
        auto error = licenseManager->removeLicenseSync(licenseObject);
        if (error != ec2::ErrorCode::ok)
        {
            NX_WARNING(this, lm("Can't remove license key %1 from the database. DB error %2")
                .args(licenseKey, error));
        }
    }

    QMap<QString, ec2::ApiDetailedLicenseData> existingLicenses;
    for (const auto& license: licenseData().licenses)
        existingLicenses.insert(license.key, license);

    QList<QnLicensePtr> updatedLicenses;
    for (const auto& licenseData: response.licenses)
    {
        if (existingLicenses.value(licenseData.key) != licenseData)
            updatedLicenses << QnLicensePtr(new QnLicense(licenseData));
    }

    auto error = licenseManager->addLicensesSync(updatedLicenses);
    if (error != ec2::ErrorCode::ok)
        NX_WARNING(this, lm("Can't update licenses into the database. DB error %1").arg(error));
}

void LicenseWatcher::stopHttpClient()
{
    decltype(m_httpClient) httpClient;
    {
        QnMutexLocker lock(&m_mutex);
        std::swap(m_httpClient, httpClient);
    }

    if (httpClient)
        httpClient->pleaseStopSync();
}

ServerLicenseInfo LicenseWatcher::licenseData() const
{
    ec2::ApiRuntimeData runtimeData = runtimeInfoManager()->items()
        ->getItem(commonModule()->moduleGUID()).data;

    ServerLicenseInfo result;
    result.info.brand = runtimeData.brand;
    result.info.version = nx::utils::AppInfo::applicationVersion();
    result.info.latestHwid = LLUtil::getLatestHardwareId();
    result.info.hwids = LLUtil::getAllHardwareIds();

    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return result; //< Server is about to stop.

    QnLicenseList licenseList;
    auto licenseManager = connection->getLicenseManager(Qn::kSystemAccess);
    if (licenseManager->getLicensesSync(&licenseList) != ec2::ErrorCode::ok)
        return result;

    for (const auto& license: licenseList)
    {
        ec2::ApiDetailedLicenseData licenseData;
        ec2::fromResourceToApi(license, licenseData);
        if (result.info.hwids.contains(licenseData.hardwareId))
            result.licenses.push_back(licenseData);
    }

    return result;
}

} // namespace mediaserver
} // namespace nx
