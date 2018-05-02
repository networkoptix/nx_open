#pragma once

#include <nx/network/http/http_types.h>

#include <core/resource/resource_fwd.h>

class QnCommonModule;
struct QnAuthSession;
struct QnJsonRestResult;
struct SetupLocalSystemData;

namespace nx {
namespace vms {
namespace utils {

class SystemSettingsProcessor;

class SetupSystemProcessor
{
public:
    SetupSystemProcessor(QnCommonModule* commonModule);
    virtual ~SetupSystemProcessor() = default;

    /**
    * Specifies custom system settings processor.
    * If not called, nx::vms::utils::SystemSettingsProcessor is used.
    */
    void setSystemSettingsProcessor(
        nx::vms::utils::SystemSettingsProcessor* systemSettingsProcessor);

    nx::network::http::StatusCode::Value setupLocalSystem(
        const QnAuthSession& authSession,
        const SetupLocalSystemData& data,
        QnJsonRestResult* result);

    QnUserResourcePtr getModifiedLocalAdmin() const;

private:
    QnCommonModule* m_commonModule;
    QnUserResourcePtr m_modifiedAdminUser;
    nx::vms::utils::SystemSettingsProcessor* m_systemSettingsProcessor = nullptr;
};

} // namespace utils
} // namespace vms
} // namespace nx
