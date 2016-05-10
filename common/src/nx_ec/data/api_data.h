#ifndef QN_API_DATA_H
#define QN_API_DATA_H

#include "api_globals.h"

namespace ec2 {

    struct ApiData {
        virtual ~ApiData() {}
    };
    #define ApiData_Fields ()

    struct ApiIdData: ApiData
    {
        ApiIdData() {}
        ApiIdData(const QnUuid& id): id(id) {}
        QnUuid id;
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

struct ApiDatabaseDumpToFileData : public ApiData {
    ApiDatabaseDumpToFileData() : size(0) {}
    qint64 size;
};
#define ApiDatabaseDumpToFileData_Fields (size)

struct ApiSyncMarkerRecord: public ApiData
{
    ApiSyncMarkerRecord(): sequence(0) {}
    QnUuid peerID;
    QnUuid dbID;
    int sequence;
};
#define ApiSyncMarkerRecord_Fields (peerID)(dbID)(sequence)

struct ApiUpdateSequenceData: public ApiData
{
    std::vector<ApiSyncMarkerRecord> markers;
};
#define ApiUpdateSequenceData_Fields (markers)


} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiDatabaseDumpData);
Q_DECLARE_METATYPE(ec2::ApiDatabaseDumpToFileData);

#endif // QN_API_DATA_H
