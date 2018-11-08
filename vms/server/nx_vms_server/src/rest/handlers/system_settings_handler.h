#pragma once

#include <nx/vms/utils/system_settings_processor.h>

#include <rest/server/json_rest_handler.h>
#include <core/resource_access/user_access_data.h>
#include <api/model/audit/auth_session.h>

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

/*!
    If no parameters are specified then just returns list of settings.
    If setting(s) to set is specified, then new values are returned
*/
class QnSystemSettingsHandler:
    public QnJsonRestHandler
{
    Q_OBJECT

public:
    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

    bool updateSettings(
        QnCommonModule* commonModule,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const Qn::UserAccessData& accessRights,
        const QnAuthSession& authSession);

private:
    void systemNameChanged(
        const QnAuthSession& authSession,
        const QString& oldValue,
        const QString& newValue);
};
