#pragma once

#include <rest/server/fusion_rest_handler.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/utils/move_only_func.h>

namespace nx::vms::server { class Settings; }
struct UpdateInformationRequestData;

class QnUpdateInformationRestHandler:
    public QnFusionRestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    QnUpdateInformationRestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor*processor) override;

private:
    using HandlerType = std::function<int(
        QByteArray* outBody,
        QByteArray* outContentType)>;

    HandlerType createHandler(const UpdateInformationRequestData& request) const;
    int handleFreeSpace(
        const UpdateInformationRequestData& request,
        QByteArray* outBody,
        QByteArray* outContentType) const;

    int handleCheckCloudHost(
        const UpdateInformationRequestData& request,
        QByteArray* outBody,
        QByteArray* outContentType) const;

    int handleVersion(
        const UpdateInformationRequestData& request,
        QByteArray* outBody,
        QByteArray* outContentType) const ;

    int makeUpdateInformationResponseFromLocalData(
        const UpdateInformationRequestData& request,
        QByteArray* result,
        QByteArray* contentType) const;

    int queryUpdateInformationAndMakeResponse(
        const UpdateInformationRequestData& request,
        QByteArray* result,
        QByteArray* contentType) const;

    qint64 freeSpaceForUpdate() const;

    int checkInternetForUpdate(
        const UpdateInformationRequestData& request,
        QByteArray* result,
        QByteArray* contentType) const;

    int checkForUpdateInformationRemotely(
        const UpdateInformationRequestData& request,
        QByteArray* result,
        QByteArray* contentType) const;

    bool serverHasInternet(const QnUuid& serverId) const;
};
