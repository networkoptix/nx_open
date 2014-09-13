#ifndef QN_API_DATA_H
#define QN_API_DATA_H

#include "api_globals.h"

namespace ec2 {
    
    struct ApiData {
        virtual ~ApiData() {}
    };
    #define ApiData_Fields ()

    struct ApiIdData: ApiData {
        QUuid id;
    };
#define ApiIdData_Fields (id)

struct ApiDataWithVersion: public ApiData {
    ApiDataWithVersion(): version(0) {}
    int version;
};
#define ApiDataWithVersion_Fields (version)

struct ApiDatabaseDumpData: public ApiData {
    QByteArray data;

};
#define ApiDatabaseDumpData_Fields (data)

struct ApiSyncMarkerRecord: public ApiData
{
    ApiSyncMarkerRecord(): sequence(0) {}
    QUuid peerID;
    QUuid dbID;
    int sequence;
};
#define ApiSyncMarkerRecord_Fields (peerID)(dbID)(sequence)

struct ApiSyncMarkerData: public ApiData 
{
    std::vector<ApiSyncMarkerRecord> markers;
};
#define ApiSyncMarkerData_Fields (markers)


} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiDatabaseDumpData);

#endif // QN_API_DATA_H
