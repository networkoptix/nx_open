#ifndef __API_RUNTIME_DATA_H_
#define __API_RUNTIME_DATA_H_

#include "api_globals.h"
#include "api_data.h"
#include "api_peer_data.h"
#include "utils/common/latin1_array.h"

namespace ec2
{
    /*
    * This structure contains all runtime data per peer. Runtime data is absent in a DB.
    */

    struct ApiRuntimeData: ApiData
    {
        ApiRuntimeData(): ApiData(), prematureLicenseExperationDate(0), version(0) {}
        bool operator==(const ApiRuntimeData& other) const {
            return peer == other.peer &&
                   platform == other.platform &&
                   box == other.box &&
                   publicIP == other.publicIP &&
                   videoWallInstanceGuid == other.videoWallInstanceGuid &&
                   prematureLicenseExperationDate == other.prematureLicenseExperationDate;
        }

        ApiPeerData peer;

        QString platform;
        QString box;
        QString brand;
        QString publicIP;
        qint64 prematureLicenseExperationDate;

        /** Guid of the videowall instance for the running videowall clients. */
        QUuid videoWallInstanceGuid;

        QList<QByteArray> mainHardwareIds;
        QList<QByteArray> compatibleHardwareIds;

        int version;
    };

#define ApiRuntimeData_Fields (peer)(platform)(box)(brand)(publicIP)(prematureLicenseExperationDate)(videoWallInstanceGuid)(mainHardwareIds)(compatibleHardwareIds)(version)


} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiRuntimeData);

#endif // __API_RUNTIME_DATA_H_
