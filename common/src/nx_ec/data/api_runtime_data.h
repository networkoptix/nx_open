#ifndef __API_RUNTIME_DATA_H_
#define __API_RUNTIME_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "api_peer_data.h"
#include "api_routing_data.h"
#include "utils/common/latin1_array.h"

namespace ec2
{
    /**
    * This structure contains all runtime data per peer. Runtime data is absent in a DB.
    */
    struct ApiRuntimeData: ApiDataWithVersion
    {
        static const quint64 tpfPeerTimeSynchronizedWithInternetServer  = 0x0008LL << 32;
        static const quint64 tpfPeerTimeSetByUser                       = 0x0004LL << 32;
        static const quint64 tpfPeerHasMonotonicClock                   = 0x0002LL << 32;
        static const quint64 tpfPeerIsNotEdgeServer                     = 0x0001LL << 32;

        ApiRuntimeData(): 
            ApiDataWithVersion(), 
            prematureLicenseExperationDate(0),
            serverTimePriority(0),
            updateStarted(false)
        {}

        bool operator==(const ApiRuntimeData& other) const {
            return version == other.version &&
                   peer == other.peer &&
                   platform == other.platform &&
                   box == other.box &&
                   publicIP == other.publicIP &&
                   videoWallInstanceGuid == other.videoWallInstanceGuid &&
                   videoWallControlSession == other.videoWallControlSession &&
                   serverTimePriority == other.serverTimePriority &&
                   prematureLicenseExperationDate == other.prematureLicenseExperationDate &&
                   availableConnections == other.availableConnections &&
                   mainHardwareIds == other.mainHardwareIds &&
                   compatibleHardwareIds == other.compatibleHardwareIds;
        }

        ApiPeerData peer;

        QString platform;
        QString box;
        QString brand;
        QString publicIP;
        qint64 prematureLicenseExperationDate;

        /** Guid of the videowall instance for the running videowall clients. */
        QnUuid videoWallInstanceGuid;

        /** Videowall item id, governed by the current client instance's control session. */
        QnUuid videoWallControlSession;

        /** Priority of this peer as the time synchronization server. */
        quint64 serverTimePriority;

        /** A list of available connections to other peers. */
        QList<ApiConnectionData> availableConnections;

        QList<QByteArray> mainHardwareIds;
        QList<QByteArray> compatibleHardwareIds;

        bool updateStarted;
    };

#define ApiRuntimeData_Fields ApiDataWithVersion_Fields (peer)(platform)(box)(brand)(publicIP)(prematureLicenseExperationDate)\
                                                        (videoWallInstanceGuid)(videoWallControlSession)(serverTimePriority)\
                                                        (availableConnections)(mainHardwareIds)(compatibleHardwareIds)


} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiRuntimeData);

#endif // __API_RUNTIME_DATA_H_
