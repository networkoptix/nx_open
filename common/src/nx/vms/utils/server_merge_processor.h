#pragma once

#include <nx/network/http/http_types.h>

#include <api/model/audit/auth_session.h>
#include <api/model/configure_system_data.h>
#include <api/model/merge_system_data.h>
#include <core/resource_access/user_access_data.h>
#include <utils/merge_systems_common.h>

class QnCommonModule;
struct QnJsonRestResult;

namespace nx {
namespace vms {
namespace utils {

class SystemMergeProcessor
{
public:
    SystemMergeProcessor(
        QnCommonModule* commonModule,
        const QString& dataDirectory);

    nx_http::StatusCode::Value merge(
        Qn::UserAccessData accessRights,
        const QnAuthSession& authSession,
        MergeSystemData data,
        QnJsonRestResult& result);

    /**
     * Valid only after successful SystemMergeProcessor::merge call.
     */
    const QnModuleInformationWithAddresses& remoteModuleInformation() const;

private:
    QnCommonModule* m_commonModule;
    const QString m_dataDirectory;
    QnAuthSession m_authSession;
    QnModuleInformationWithAddresses m_remoteModuleInformation;

    void setMergeError(
        QnJsonRestResult& result,
        ::utils::MergeSystemsStatus::Value mergeStatus);

    bool applyCurrentSettings(
        const QUrl &remoteUrl,
        const QString& getKey,
        const QString& postKey,
        bool oneServer);

    bool applyRemoteSettings(
        const QUrl &remoteUrl,
        const QnUuid& systemId,
        const QString& systemName,
        const QString& getKey,
        const QString& postKey);

    bool executeRemoteConfigure(
        const ConfigureSystemData& data,
        const QUrl &remoteUrl,
        const QString& postKey);
};

} // namespace utils
} // namespace vms
} // namespace nx
