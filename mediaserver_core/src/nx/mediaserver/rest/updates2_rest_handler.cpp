#include <nx/update/info/sync_update_checker.h>
#include "updates2_rest_handler.h"
#include <utils/common/app_info.h>
#include <nx/network/app_info.h>
#include <nx/api/updates2/available_update_info_data.h>


namespace nx {
namespace mediaserver {
namespace rest {

UpdateRequestDataFactory::FactoryFunc UpdateRequestDataFactory::s_factoryFunc = nullptr;

update::info::UpdateRequestData UpdateRequestDataFactory::create()
{
    if (s_factoryFunc)
        return s_factoryFunc();

    return update::info::UpdateRequestData(
        nx::network::AppInfo::defaultCloudHost(),
        QnAppInfo::customizationName(),
        QnSoftwareVersion(QnAppInfo::applicationVersion()));
}

void UpdateRequestDataFactory::setFactoryFunc(UpdateRequestDataFactory::FactoryFunc factoryFunc)
{
    s_factoryFunc = std::move(factoryFunc);
}

namespace {

class GetUpdateInfoHelper
{
public:
    GetUpdateInfoHelper()
    {
        fillNegativeRestResponse();
        if (!findUpdateInfo())
            return;

        fillPositiveRestResponse();
    }

    JsonRestResponse response() const { return m_jsonRestResponse; }
private:
    JsonRestResponse m_jsonRestResponse;
    QnSoftwareVersion m_version;

    bool findUpdateInfo()
    {
        const update::info::AbstractUpdateRegistryPtr updateRegistry = update::info::checkSync();
        if (updateRegistry == nullptr)
            return false;

        const auto findResult = updateRegistry->latestUpdate(
            UpdateRequestDataFactory::create(),
            &m_version);
        return findResult == update::info::ResultCode::ok;
    }

    void fillNegativeRestResponse() { fillRestResponse(); }
    void fillPositiveRestResponse() { fillRestResponse(m_version.toString()); }

    void fillRestResponse(const QString& version = QString())
    {
        m_jsonRestResponse.statusCode = nx_http::StatusCode::ok;
        m_jsonRestResponse.json.setReply(api::AvailableUpdatesInfo(version));
    }
};

static const QString kUpdates2Path = "/api/updates2";
} // namespace

JsonRestResponse Updates2RestHandler::executeGet(const JsonRestRequest& request)
{
    NX_ASSERT(request.path == kUpdates2Path);
    if (request.path != kUpdates2Path)
        return nx_http::StatusCode::notFound;

    return GetUpdateInfoHelper().response();
}

JsonRestResponse Updates2RestHandler::executeDelete(const JsonRestRequest& request)
{
    return JsonRestResponse();
}

JsonRestResponse Updates2RestHandler::executePost(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    return JsonRestResponse();
}

JsonRestResponse Updates2RestHandler::executePut(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    return JsonRestResponse();
}

} // namespace rest
} // namespace mediaserver
} // namespace nx
