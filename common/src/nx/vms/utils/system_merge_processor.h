#pragma once

#include <api/model/audit/auth_session.h>
#include <api/model/configure_system_data.h>
#include <api/model/merge_system_data.h>
#include <core/resource_access/user_access_data.h>
#include <utils/merge_systems_common.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/http_types.h>
#include <nx/vms/api/data/module_information.h>

class QnCommonModule;
struct QnJsonRestResult;
class MediaServerClient;

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

    QnJsonRestResult merge(
        Qn::UserAccessData accessRights,
        const QnAuthSession& authSession,
        MergeSystemData data);

    /**
     * Valid only after successful SystemMergeProcessor::merge call.
     */
    const nx::vms::api::ModuleInformationWithAddresses& remoteModuleInformation() const;

private:
    QnCommonModule* m_commonModule;
    QString m_dataDirectory;
    QnAuthSession m_authSession;
    nx::vms::api::ModuleInformation m_localModuleInformation;
    nx::vms::api::ModuleInformationWithAddresses m_remoteModuleInformation;
    bool m_dbBackupEnabled = false;
    nx::String m_cloudAuthKey;

    bool validateInputData(
        const MergeSystemData& data,
        QnJsonRestResult* result);

    void saveBackupOfSomeLocalData();

    QnJsonRestResult checkWhetherMergeIsPossible(
        const MergeSystemData& data);

    QnJsonRestResult checkIfSystemsHaveServerWithSameId(
        MediaServerClient* remoteMediaServerClient);

    QnJsonRestResult checkIfCloudSystemsMergeIsPossible(
        const MergeSystemData& data,
        MediaServerClient* remoteMediaServerClient);

    QnJsonRestResult mergeSystems(
        Qn::UserAccessData accessRights,
        MergeSystemData data);

    void setMergeError(
        QnJsonRestResult* result,
        ::utils::MergeSystemsStatus::Value mergeStatus);

    nx::network::http::StatusCode::Value applyCurrentSettings(
        const nx::utils::Url& remoteUrl,
        const QString& postKey,
        bool oneServer);

    nx::network::http::StatusCode::Value applyRemoteSettings(
        const nx::utils::Url& remoteUrl,
        const QnUuid& systemId,
        const QString& systemName,
        const QString& getKey,
        const QString& postKey);

    nx::network::http::StatusCode::Value executeRemoteConfigure(
        const ConfigureSystemData& data,
        const nx::utils::Url &remoteUrl,
        const QString& postKey);

    bool isResponseOK(const nx::network::http::HttpClient& client);

    nx::network::http::StatusCode::Value getClientResponse(
        const nx::network::http::HttpClient& client);

    template <class ResultDataType>
    bool executeRequest(
        const nx::utils::Url &remoteUrl,
        const QString& getKey,
        ResultDataType& result,
        const QString& path);

    void addAuthToRequest(nx::utils::Url& request, const QString& remoteAuthKey);

    nx::network::http::StatusCode::Value fetchModuleInformation(
        const nx::utils::Url &url,
        const QString& authenticationKey,
        nx::vms::api::ModuleInformationWithAddresses* moduleInformation);

    bool addMergeHistoryRecord(const MergeSystemData& data);
};

} // namespace utils
} // namespace vms
} // namespace nx
