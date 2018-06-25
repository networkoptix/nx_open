#pragma once

#include "api_globals.h"
#include "api_data.h"
#include "api_peer_data.h"
#include "api_routing_data.h"
#include "nx/utils/latin1_array.h"
#include <nx/fusion/model_functions_fwd.h>

namespace ec2 {

enum RuntimeFlag
{
    RF_None = 0x000,
    RF_MasterCloudSync = 0x0001 /**< Sync transactions with cloud */
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(RuntimeFlag)
Q_DECLARE_FLAGS(RuntimeFlags, RuntimeFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(RuntimeFlags)

/**
 * This structure contains all runtime data per peer. Runtime data is absent in a DB.
 */
struct ApiRuntimeData: nx::vms::api::DataWithVersion
{
    static const quint64 tpfPeerTimeSynchronizedWithInternetServer = 0x0008LL << 32;
    static const quint64 tpfPeerTimeSetByUser = 0x0004LL << 32;
    static const quint64 tpfPeerHasMonotonicClock = 0x0002LL << 32;
    static const quint64 tpfPeerIsNotEdgeServer = 0x0001LL << 32;

    /**
     * This operator must not be replaced with fusion implementation as it skips brand and
     * customization checking.
     */
    bool operator==(const ApiRuntimeData& other) const
    {
        return version == other.version
            && peer == other.peer
            && platform == other.platform
            && box == other.box
            && publicIP == other.publicIP
            && videoWallInstanceGuid == other.videoWallInstanceGuid
            && videoWallControlSession == other.videoWallControlSession
            && prematureLicenseExperationDate == other.prematureLicenseExperationDate
            && hardwareIds == other.hardwareIds
            && updateStarted == other.updateStarted
            && nx1mac == other.nx1mac
            && nx1serial == other.nx1serial
            && userId == other.userId
            && flags == other.flags;
    }

    ApiPeerData peer;

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

    RuntimeFlags flags = RF_None;
};

#define ApiRuntimeData_Fields DataWithVersion_Fields \
    (peer)(platform)(box)(brand)(publicIP)(prematureLicenseExperationDate) \
    (videoWallInstanceGuid)(videoWallControlSession) \
    (hardwareIds)(updateStarted)(nx1mac)(nx1serial) \
    (userId)(flags)(customization)

} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiRuntimeData);

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ec2::RuntimeFlag)(ec2::RuntimeFlags),
    (metatype)(numeric)
)
