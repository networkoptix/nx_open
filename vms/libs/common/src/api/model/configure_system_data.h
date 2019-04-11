#pragma once

#include "password_data.h"
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/timestamp.h>

struct ConfigureSystemData: public PasswordData
{
    QnUuid localSystemId;
    bool wholeSystem = true;
    qint64 sysIdTime = 0;
    nx::vms::api::Timestamp tranLogTime;
    int port = 0;
    nx::vms::api::MediaServerData foreignServer;
    std::vector<nx::vms::api::UserData> foreignUsers;
    nx::vms::api::ResourceParamDataList foreignSettings;
    nx::vms::api::ResourceParamWithRefDataList additionParams;
    bool rewriteLocalSettings = false;
    QString systemName; //added for compatibility with NxTool
    QString currentPassword; // required for password change only
};

#define ConfigureSystemData_Fields PasswordData_Fields \
    (localSystemId)(wholeSystem)(sysIdTime)(tranLogTime)(port)(foreignServer)(foreignUsers) \
    (foreignSettings)(additionParams)(rewriteLocalSettings)(systemName)(currentPassword)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ConfigureSystemData), (json)(eq));
