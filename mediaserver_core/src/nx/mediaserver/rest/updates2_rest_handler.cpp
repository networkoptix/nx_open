#include <nx/update/info/sync_update_checker.h>
#include "updates2_rest_handler.h"
#include <utils/common/app_info.h>
#include <nx/network/app_info.h>
#include <nx/api/updates2/available_update_info_data.h>


namespace nx {
namespace mediaserver {
namespace rest {

namespace {

class GetUpdateInfoHelper
{
public:
    GetUpdateInfoHelper()
    {
        setNegativeResponseBody();
        m_updateRegistry = update::info::checkSync();
        if (m_updateRegistry == nullptr)
            return;

        if (findUpdateInfo())
            QJson::serialize(api::AvailableUpdatesInfo(m_version.toString()), &m_responseBody);
    }

    QByteArray responseBody() const { return m_responseBody; }

private:
    update::info::AbstractUpdateRegistryPtr m_updateRegistry;
    QByteArray m_responseBody;
    QnSoftwareVersion m_version;

    bool findUpdateInfo()
    {
        const auto findResult = m_updateRegistry->latestUpdate(
            createUpdateRequestData(),
            &m_version);
        return findResult == update::info::ResultCode::ok;
    }

    static update::info::UpdateRequestData createUpdateRequestData()
    {
        return update::info::UpdateRequestData(
            nx::network::AppInfo::defaultCloudHost(),
            QnAppInfo::customizationName(),
            QnSoftwareVersion(QnAppInfo::applicationVersion()));
    }

    void setNegativeResponseBody()
    {
        QJson::serialize(api::AvailableUpdatesInfo(), &m_responseBody);
    }
};

static const QString kUpdates2Path = "/updates2";
} // namespace

int Updates2RestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& /*params*/,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    NX_ASSERT(path == kUpdates2Path);
    if (path != kUpdates2Path)
        return nx_http::StatusCode::notFound;

    GetUpdateInfoHelper getUpdateInfoHelper;
    result = getUpdateInfoHelper.responseBody();

    return nx_http::StatusCode::ok;
}

int Updates2RestHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& bodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    NX_ASSERT(false);
    return nx_http::StatusCode::internalServerError;
}

int Updates2RestHandler::executePut(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& bodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    NX_ASSERT(false);
    return nx_http::StatusCode::internalServerError;
}

int Updates2RestHandler::executeDelete(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    NX_ASSERT(false);
    return nx_http::StatusCode::internalServerError;
}

} // namespace rest
} // namespace mediaserver
} // namespace nx
