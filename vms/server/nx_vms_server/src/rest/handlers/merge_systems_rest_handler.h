#pragma once

#include <api/model/configure_system_data.h>
#include <rest/server/json_rest_handler.h>
#include <core/resource/resource_fwd.h>
#include <utils/merge_systems_common.h>

#include <nx/vms/api/data_fwd.h>
#include <nx/vms/server/server_module_aware.h>

struct MergeSystemData;

namespace ec2 { class AbstractTransactionMessageBus; }

class QnCommonModule;

class QnMergeSystemsRestHandler:
    public QnJsonRestHandler, public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT

public:
    QnMergeSystemsRestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor *owner) override;

    virtual int executePost(
        const QString &path,
        const QnRequestParams &params,
        const QByteArray &body,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*owner) override;
private:
    int execute(
        MergeSystemData data,
        const QnRestConnectionProcessor* owner,
        QnJsonRestResult &result);

private:
    ec2::AbstractTransactionMessageBus* m_messageBus;

    void updateLocalServerAuthKeyInConfig(
        QnCommonModule* commonModule);

    void initiateConnectionToRemoteServer(
        QnCommonModule* commonModule,
        const QUrl& remoteModuleUrl,
        const nx::vms::api::ModuleInformationWithAddresses& remoteModuleInformation);
};
