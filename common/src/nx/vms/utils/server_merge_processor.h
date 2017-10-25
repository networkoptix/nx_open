#pragma once

#include <nx/network/http/http_client.h>
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
    SystemMergeProcessor(QnCommonModule* commonModule);

    /**
     * Disabled by default.
     */
    void enableDbBackup(const QString& dataDirectory);

    nx_http::StatusCode::Value merge(
        Qn::UserAccessData accessRights,
        const QnAuthSession& authSession,
        MergeSystemData data,
        QnJsonRestResult* result);

    /**
     * Valid only after successful SystemMergeProcessor::merge call.
     */
    const QnModuleInformationWithAddresses& remoteModuleInformation() const;

private:
    QnCommonModule* m_commonModule;
    QString m_dataDirectory;
    QnAuthSession m_authSession;
    QnModuleInformationWithAddresses m_remoteModuleInformation;
    bool m_dbBackupEnabled = false;

    void setMergeError(
        QnJsonRestResult* result,
        ::utils::MergeSystemsStatus::Value mergeStatus);

    bool applyCurrentSettings(
        const nx::utils::Url& remoteUrl,
        const QString& postKey,
        bool oneServer);

    bool applyRemoteSettings(
        const nx::utils::Url& remoteUrl,
        const QnUuid& systemId,
        const QString& systemName,
        const QString& getKey,
        const QString& postKey);

    bool executeRemoteConfigure(
        const ConfigureSystemData& data,
        const nx::utils::Url &remoteUrl,
        const QString& postKey);

    bool isResponseOK(const nx_http::HttpClient& client);

    nx_http::StatusCode::Value getClientResponse(const nx_http::HttpClient& client);

    template <class ResultDataType>
    bool executeRequest(
        const nx::utils::Url &remoteUrl,
        const QString& getKey,
        ResultDataType& result,
        const QString& path);

    void addAuthToRequest(nx::utils::Url& request, const QString& remoteAuthKey);
};

} // namespace utils
} // namespace vms
} // namespace nx
