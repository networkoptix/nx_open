// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/timestamp.h>
#include <nx/vms/api/data/user_data.h>

#include "password_data.h"

struct NX_VMS_COMMON_API ConfigureSystemData: public PasswordData
{
    nx::Uuid localSystemId;
    QString cloudSystemId;
    bool wholeSystem = false;
    qint64 sysIdTime = 0;
    nx::vms::api::Timestamp tranLogTime;
    int port = 0;

    // The server from a claster A that call /api/configure to the the server on a claster B.
    nx::vms::api::MediaServerData foreignServer;
    std::vector<nx::vms::api::UserData> foreignUsers;
    nx::vms::api::ResourceParamDataList foreignSettings;
    bool rewriteLocalSettings = false;
    QString systemName; //added for compatibility with NxTool
    QString currentPassword; // required for password change only
    nx::Uuid mergeId;
    std::set<nx::Uuid> remoteRemovedObjects;
    bool isLocal = false;

    // All servers from a cluster A.
    std::set<nx::Uuid> foreignServers;

    bool operator==(const ConfigureSystemData& other) const = default;
};

#define ConfigureSystemData_Fields PasswordData_Fields \
    (localSystemId) \
    (cloudSystemId) \
    (wholeSystem) \
    (sysIdTime) \
    (tranLogTime) \
    (port) \
    (foreignServer) \
    (foreignUsers) \
    (foreignSettings) \
    (rewriteLocalSettings) \
    (systemName) \
    (currentPassword) \
    (mergeId) \
    (remoteRemovedObjects) \
    (foreignServers) \
    (isLocal)

QN_FUSION_DECLARE_FUNCTIONS(ConfigureSystemData, (json), NX_VMS_COMMON_API)
