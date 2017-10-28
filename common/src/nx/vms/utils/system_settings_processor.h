#pragma once

#include <nx/network/http/http_types.h>
#include <nx/utils/move_only_func.h>

#include <api/model/audit/auth_session.h>
#include <core/resource_access/user_access_data.h>
#include <utils/common/request_param.h>

class QnCommonModule;
struct QnJsonRestResult;

namespace nx {
namespace vms {
namespace utils {

class SystemSettingsProcessor
{
public:
    using BeforeUpdatingSettingValueHandler = nx::utils::MoveOnlyFunc<void(
        const QString& /*name*/,
        const QString& /*oldValue*/,
        const QString& /*newValue*/)>;

    SystemSettingsProcessor(QnCommonModule* commonModule);
    virtual ~SystemSettingsProcessor() = default;

    void setOnBeforeUpdatingSettingValue(BeforeUpdatingSettingValueHandler handler);

    nx_http::StatusCode::Value updateSettings(
        const Qn::UserAccessData& accessRights,
        const QnAuthSession& authSession,
        const QnRequestParams& params,
        QnJsonRestResult* result);

private:
    QnCommonModule* m_commonModule = nullptr;
    BeforeUpdatingSettingValueHandler m_onBeforeUpdatingSettingValue;
};

} // namespace utils
} // namespace vms
} // namespace nx
