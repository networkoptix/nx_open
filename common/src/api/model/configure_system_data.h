#pragma once

#include "password_data.h"
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx_ec/data/api_media_server_data.h>
#include <nx_ec/transaction_timestamp.h>
#include <nx_ec/data/api_user_data.h>

struct ConfigureSystemData: public PasswordData
{
    ConfigureSystemData():
        PasswordData(),
        wholeSystem(false),
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
        systemName(params.value(lit("systemName")))
    {
        tranLogTime.sequence = params.value(lit("tranLogTimeSequence")).toULongLong();
        tranLogTime.ticks = params.value(lit("tranLogTimeTicks")).toULongLong();
    }

    QnUuid localSystemId;
    bool wholeSystem;
    qint64 sysIdTime;
    ec2::Timestamp tranLogTime;
    int port;
    ec2::ApiMediaServerData foreignServer;
    std::vector<ec2::ApiUserData> foreignUsers;
    ec2::ApiResourceParamDataList foreignSettings;
    ec2::ApiResourceParamWithRefDataList additionParams;
    bool rewriteLocalSettings;
    QString systemName; //added for compatibility with NxTool
};

#define ConfigureSystemData_Fields PasswordData_Fields (localSystemId)(wholeSystem)(sysIdTime)(tranLogTime)(port)(foreignServer)(foreignUsers)(foreignSettings)(additionParams)(rewriteLocalSettings)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ConfigureSystemData), (json));
