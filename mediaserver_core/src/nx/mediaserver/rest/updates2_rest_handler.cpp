#include <nx/update/info/sync_update_checker.h>
#include "updates2_rest_handler.h"
#include <utils/common/app_info.h>
#include <nx/network/app_info.h>


namespace nx {
namespace mediaserver {
namespace rest {

namespace {

class GetUpdateInfoHelper
{
public:
    GetUpdateInfoHelper()
    {
        m_updateRegistry = update::info::checkSync();
        if (m_updateRegistry == nullptr)
        {
            m_statusCode = nx_http::StatusCode::internalServerError;
            return;
        }

        if (findUpdateInfo())
        {
            setResponseBody();
            m_statusCode = nx_http::StatusCode::ok;
        }
    }

    nx_http::StatusCode::Value statusCode() const { return m_statusCode; }
    QByteArray responseBody() const { return m_responseBody; }

private:
    update::info::AbstractUpdateRegistryPtr m_updateRegistry;
    update::info::FileData m_fileData;
    QByteArray m_responseBody;
    nx_http::StatusCode::Value m_statusCode = nx_http::StatusCode::notFound;

    bool findUpdateInfo()
    {
        const auto findResult = m_updateRegistry->findUpdateFile(
            createUpdateRequestData(),
            &m_fileData);
        return findResult == update::info::ResultCode::ok;
    }

    static update::info::UpdateFileRequestData createUpdateRequestData()
    {
        return update::info::UpdateFileRequestData(
            nx::network::AppInfo::defaultCloudHost(),
            QnAppInfo::customizationName(),
            QnSoftwareVersion(QnAppInfo::applicationVersion()),
            update::info::OsVersion(
                QnAppInfo::applicationPlatform(),
                QnAppInfo::applicationArch(),
                QnAppInfo::applicationPlatformModification()));
    }

    void setResponseBody()
    {
        m_responseBody = QJson::serialize(m_fileData.);
    }
};

} // namespace

int Updates2RestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& /*params*/,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    NX_ASSERT(path == "/updates2");
    auto updateRegistry = update::info::checkSync();
    if (updateRegistry == nullptr)
        return nx_http::StatusCode::notFound;
    update::info::FileData fileData;
    updateRegistry->findUpdateFile(createUpdateRequestData(), &fileData);
    return nx_http::StatusCode::internalServerError;
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
