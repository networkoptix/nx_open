#pragma once

#include "data_with_version.h"
#include "peer_data.h"

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QVector>

#include <nx/utils/uuid.h>
#include <nx/vms/api/types/connection_types.h>

namespace nx {
namespace vms {
namespace api {

/**
 * This structure contains all runtime data per peer. Runtime data is absent in a DB.
 */
struct NX_VMS_API RuntimeData: DataWithVersion
{
    PeerData peer;

    QString platform;
    QString box;
    QString brand;
    QString customization;
    QString publicIP;
    qint64 prematureLicenseExperationDate = 0;

    /** Guid of the videowall instance for the running videowall clients. */
    QnUuid videoWallInstanceGuid;

    /** Videowall item id, governed by the current client instance's control session. */
    QnUuid videoWallControlSession;

    QVector<QString> hardwareIds;

    QString nx1mac;
    QString nx1serial;

    bool updateStarted = false;

    /** Id of the user, under which peer is logged in (for client peers only) */
    QnUuid userId;

    RuntimeFlags flags = {};

    QSet<QnUuid> activeAnalyticsEngines;

    /**
    * This operator must not be replaced with fusion implementation as it skips brand and
    * customization checking.
    */
    bool operator==(const RuntimeData& other) const;
    bool operator!=(const RuntimeData& other) const { return !operator==(other); }
};

#define RuntimeData_Fields \
    DataWithVersion_Fields \
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
    (activeAnalyticsEngines)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::RuntimeData)
