#pragma once

#include "password_data.h"
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/timestamp.h>

struct ConfigureSystemData: public PasswordData
{
    ConfigureSystemData():
        PasswordData(),
        wholeSystem(true),
        sysIdTime(0),
        port(0),
        rewriteLocalSettings(false)
    {
    }

    ConfigureSystemData(const QnRequestParams& params) :
        PasswordData(params),
        localSystemId(params.value(lit("localSystemId"))),
        wholeSystem(params.value(lit("wholeSystem"), lit("false")) != lit("false")),
        sysIdTime(params.value(lit("sysIdTime")).toLongLong()),
        //tranLogTime(params.value(lit("tranLogTime")).toLongLong()),
        port(params.value(lit("port")).toInt()),
        systemName(params.value(lit("systemName"))),
        currentPassword(params.value(lit("currentPassword")))
    {
        tranLogTime.sequence = params.value(lit("tranLogTimeSequence")).toULongLong();
        tranLogTime.ticks = params.value(lit("tranLogTimeTicks")).toULongLong();
        if (currentPassword.isEmpty())
            currentPassword = params.value(lit("oldPassword")); //< Compatibility with previous version.
    }

    QnUuid localSystemId;
    bool wholeSystem;
    qint64 sysIdTime;
    nx::vms::api::Timestamp tranLogTime;
    int port;
    nx::vms::api::MediaServerData foreignServer;
    std::vector<nx::vms::api::UserData> foreignUsers;
    nx::vms::api::ResourceParamDataList foreignSettings;
    nx::vms::api::ResourceParamWithRefDataList additionParams;
    bool rewriteLocalSettings;
    QString systemName; //added for compatibility with NxTool
    QString currentPassword; // required for password change only
    QnUuid mergeId;
};

#define ConfigureSystemData_Fields PasswordData_Fields (localSystemId)(wholeSystem)(sysIdTime)(tranLogTime)(port)(foreignServer)(foreignUsers)(foreignSettings)(additionParams)(rewriteLocalSettings)(systemName)(currentPassword)(mergeId)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ConfigureSystemData), (json)(eq));
