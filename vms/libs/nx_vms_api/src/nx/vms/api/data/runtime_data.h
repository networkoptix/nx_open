// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QtGlobal>

#include <nx/utils/uuid.h>
#include <nx/vms/api/types/connection_types.h>

#include "data_macros.h"
#include "peer_data.h"

namespace nx::vms::api {

/**%apidoc All runtime data of the peer. Runtime data is absent in a DB. */
struct NX_VMS_API RuntimeData
{
    int version = 0;

    PeerData peer;

    QString platform;
    QString box;
    QString brand;
    QString customization;
    QString publicIP;
    qint64 prematureLicenseExperationDate = 0;

    /**%apidoc Guid of the videowall instance for the running videowall clients. */
    QnUuid videoWallInstanceGuid;

    /**%apidoc Videowall layout id, governed by the current client instance's control session. */
    QnUuid videoWallControlSession;

    QVector<QString> hardwareIds;

    QString nx1mac;
    QString nx1serial;

    bool updateStarted = false;

    /**%apidoc Id of the user, under which peer is logged in (for client peers only). */
    QnUuid userId;

    RuntimeFlags flags = {};

    /**%apidoc:uuidArray */
    QSet<QnUuid> activeAnalyticsEngines;

    qint64 prematureVideoWallLicenseExpirationDate = 0;

    /**
    * This operator must not be replaced with reflection implementation as it skips brand and
    * customization checking.
    */
    bool operator==(const RuntimeData& other) const;
    bool operator!=(const RuntimeData& other) const { return !operator==(other); }
};

#define RuntimeData_Fields \
    (version) \
    (peer) \
    (platform) \
    (box) \
    (brand) \
    (publicIP) \
    (prematureLicenseExperationDate) \
    (videoWallInstanceGuid) \
    (videoWallControlSession) \
    (hardwareIds) \
    (updateStarted) \
    (nx1mac) \
    (nx1serial) \
    (userId) \
    (flags) \
    (customization) \
    (activeAnalyticsEngines) \
    (prematureVideoWallLicenseExpirationDate)
NX_VMS_API_DECLARE_STRUCT_EX(RuntimeData, (ubjson)(json)(xml))

} // namespace nx::vms::api
