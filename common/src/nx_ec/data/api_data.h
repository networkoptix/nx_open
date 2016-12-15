#pragma once

#include "api_globals.h"

namespace ec2 {

struct ApiData
{
    virtual ~ApiData() {}
};
#define ApiData_Fields ()

struct ApiIdData: ApiData
{
    ApiIdData() {}
    ApiIdData(const QnUuid& id): id(id) {}

    /**
     * Subclasses can (re)define this method when needed. It is called by name from templates
     * via sfinae, if this method is defined, hence it is non-virtual, and any serializable
     * class which is not inherited from ApiIdData can also define such method.
     * Used for merging with the existing object in POST requests.
     * @return Value of a field holding object guid.
     */
    QnUuid getIdForMerging() const { return id; }

    /**
     * Subclasses can (re)define this method when needed. It is called by name from templates
     * via sfinae, if this method is defined, hence it is non-virtual, and any serializable
     * class which is not inherited from ApiIdData can also define such method together with
     * defining getIdForMerging() (otherwise, this method will not be called).
     * Used for generating ommitted id in POST requests which create new objects. Can set id to
     * a null guid if generating is not possible.
     */
    void fillId() { id = QnUuid::createUuid(); }

    bool operator==(const ApiIdData& rhs) const;

    QnUuid id;
};
#define ApiIdData_Fields (id)

struct ApiDataWithVersion: ApiData
{
    ApiDataWithVersion(): version(0) {}
    int version;
};
#define ApiDataWithVersion_Fields (version)

struct ApiDatabaseDumpData: ApiData
{
    QByteArray data;
};
#define ApiDatabaseDumpData_Fields (data)

struct ApiDatabaseDumpToFileData: ApiData
{
    ApiDatabaseDumpToFileData(): size(0) {}
    qint64 size;
};
#define ApiDatabaseDumpToFileData_Fields (size)

struct ApiSyncMarkerRecord: ApiData
{
    ApiSyncMarkerRecord(): sequence(0) {}
    QnUuid peerID;
    QnUuid dbID;
    int sequence;
};
#define ApiSyncMarkerRecord_Fields (peerID)(dbID)(sequence)

struct ApiUpdateSequenceData: ApiData
{
    std::vector<ApiSyncMarkerRecord> markers;
};
#define ApiUpdateSequenceData_Fields (markers)

} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiDatabaseDumpData);
Q_DECLARE_METATYPE(ec2::ApiDatabaseDumpToFileData);
