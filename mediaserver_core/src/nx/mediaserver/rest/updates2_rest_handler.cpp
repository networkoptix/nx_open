#include <nx/update/info/sync_update_checker.h>
#include "updates2_rest_handler.h"
#include <utils/common/app_info.h>
#include <nx/network/app_info.h>


namespace nx {
namespace mediaserver {
namespace rest {

namespace {
static update::info::UpdateRequestData createUpdateRequestData()
{
    return update::info::UpdateRequestData(
        nx::network::AppInfo::defaultCloudHost(),
        QnAppInfo::customizationName(),
        QnSoftwareVersion(QnAppInfo::applicationVersion()),
        update::info::OsVersion(
            QnAppInfo::applicationPlatform(),
            QnAppInfo::applicationArch(),
            QnAppInfo::applicationPlatformModification()));
}
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
    updateRegistry->findUpdate(createUpdateRequestData(), &fileData);
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
