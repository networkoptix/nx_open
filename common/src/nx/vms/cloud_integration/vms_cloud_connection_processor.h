#pragma once

#include <nx/network/http/http_types.h>

class QnCommonModule;
struct CloudCredentialsData;
struct DetachFromCloudData;
struct DetachFromCloudReply;
struct QnJsonRestResult;
struct QnAuthSession;
struct SetupCloudSystemData;

namespace nx {
namespace vms {

namespace utils { class SystemSettingsProcessor; }

namespace cloud_integration {

struct CloudManagerGroup;
class SystemSettingsProcessor;

class VmsCloudConnectionProcessor
{
public:
    VmsCloudConnectionProcessor(
        QnCommonModule* commonModule,
        CloudManagerGroup* cloudManagerGroup);

    void setSystemSettingsProcessor(
        nx::vms::utils::SystemSettingsProcessor* systemSettingsProcessor);

    nx_http::StatusCode::Value bindSystemToCloud(
        const CloudCredentialsData& data,
        QnJsonRestResult* result);

    nx_http::StatusCode::Value setupCloudSystem(
        const QnAuthSession& authSession,
        const SetupCloudSystemData& data,
        QnJsonRestResult* result);

    nx_http::StatusCode::Value detachFromCloud(
        const DetachFromCloudData& data,
        DetachFromCloudReply* result);

    QString errorDescription() const;

private:
    QnCommonModule* m_commonModule;
    CloudManagerGroup* m_cloudManagerGroup;
    QString m_errorDescription;
    nx::vms::utils::SystemSettingsProcessor* m_systemSettingsProcessor = nullptr;

    bool validateInputData(
        const CloudCredentialsData& data,
        QnJsonRestResult* result);
    bool checkInternetConnection(QnJsonRestResult* result);

    bool saveCloudData(
        const CloudCredentialsData& data,
        QnJsonRestResult* result);
    bool saveCloudCredentials(
        const CloudCredentialsData& data,
        QnJsonRestResult* result);
    bool insertCloudOwner(
        const CloudCredentialsData& data,
        QnJsonRestResult* result);

    bool fetchNecessaryDataFromCloud(
        const CloudCredentialsData& data,
        QnJsonRestResult* result);
    bool saveLocalSystemIdToCloud(
        const CloudCredentialsData& data,
        QnJsonRestResult* result);
    bool initializeCloudRelatedManagers(
        const CloudCredentialsData& data,
        QnJsonRestResult* result);

    bool rollback();
};

} // namespace cloud_integration
} // namespace vms
} // namespace nx
