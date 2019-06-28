#pragma once

#include <api/model/audit/auth_session.h>
#include <core/resource_access/user_access_data.h>
#include <nx/network/rest/handler.h>
#include <nx/vms/utils/system_settings_processor.h>

namespace nx::vms::server {

class SystemSettingsProcessor:
    public nx::vms::utils::SystemSettingsProcessor
{
    using base_type = nx::vms::utils::SystemSettingsProcessor;

public:
    SystemSettingsProcessor(
        QnCommonModule* commonModule,
        const QnAuthSession& authSession);

private:
    const QnAuthSession m_authSession;

    void systemNameChanged(
        const QString& oldValue,
        const QString& newValue);
};

class SystemSettingsHandler: public nx::network::rest::Handler
{
protected:
    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;

    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;

private:
    bool updateSettings(
        QnCommonModule* commonModule,
        const nx::network::rest::Params& params,
        nx::network::rest::JsonResult& result,
        const Qn::UserAccessData& accessRights,
        const QnAuthSession& authSession);

    void systemNameChanged(
        const QnAuthSession& authSession,
        const QString& oldValue,
        const QString& newValue);
};

} // namespace nx::vms::server
