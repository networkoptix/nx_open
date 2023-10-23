// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>
#include <QtCore/QString>

#include <nx/utils/serialization/format.h>
#include <nx/vms/api/types/connection_types.h>

#include "id_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API PersistentIdData: IdData
{
    /** Unique persistent Db ID of the peer. Empty for clients. */
    QnUuid persistentId;

    PersistentIdData() = default;
    PersistentIdData(const QnUuid& id, const QnUuid& persistentId);

    bool operator==(const PersistentIdData& other) const = default;

    bool isNull() const { return id.isNull(); }

    bool operator<(const PersistentIdData& other) const;
    bool operator>(const PersistentIdData& other) const { return other < *this; }
};
#define PersistentIdData_Fields \
    IdData_Fields \
    (persistentId)
NX_VMS_API_DECLARE_STRUCT(PersistentIdData)

struct NX_VMS_API PeerData: PersistentIdData
{
    PeerData();

    PeerData(
        const QnUuid& id,
        const QnUuid& instanceId,
        PeerType peerType,
        Qn::SerializationFormat dataFormat = Qn::SerializationFormat::ubjson);

    PeerData(
        const PersistentIdData& peer,
        PeerType peerType,
        Qn::SerializationFormat dataFormat = Qn::SerializationFormat::ubjson);

    PeerData(
        const QnUuid& id,
        const QnUuid& instanceId,
        const QnUuid& persistentId,
        PeerType peerType,
        Qn::SerializationFormat dataFormat = Qn::SerializationFormat::ubjson);

    bool operator==(const PeerData& other) const = default;

    bool isClient() const { return isClient(peerType); }
    bool isServer() const { return isServer(peerType); }
    bool isCloudServer() const { return isCloudServer(peerType); }
    bool isMobileClient() const { return isMobileClient(peerType); }

    static bool isServer(PeerType peerType);
    static bool isClient(PeerType peerType);
    static bool isMobileClient(PeerType peerType);
    static bool isCloudServer(PeerType peerType);

    /**%apidoc Unique running instance ID of the peer. */
    QnUuid instanceId;

    /**%apidoc Type of the peer. */
    PeerType peerType{PeerType::notDefined};

    /**%apidoc[opt]:enum
     * Preferred client data serialization format
     * %value JsonFormat
     * %value UbjsonFormat
     * %value CsvFormat
     * %value XmlFormat
     * %value CompressedPeriodsFormat
     * %value UrlQueryFormat
     * %value UrlEncodedFormat
     * %value UnsupportedFormat
     */
    Qn::SerializationFormat dataFormat = Qn::SerializationFormat::ubjson;
};
#define PeerData_Fields \
    PersistentIdData_Fields \
    (instanceId) \
    (peerType) \
    (dataFormat)
NX_VMS_API_DECLARE_STRUCT(PeerData)

using PeerSet = QSet<QnUuid>;

struct PeerDataEx: public PeerData
{
    QnUuid systemId;
    QString cloudHost;
    qint64 identityTime = 0;
    int aliveUpdateIntervalMs = 0;
    int protoVersion = 0;
    QnUuid connectionGuid;

    bool operator==(const PeerDataEx& other) const = default;
    void assign(const PeerData& data) { PeerData::operator=(data); }
};
#define PeerDataEx_Fields \
    PeerData_Fields \
    (systemId) \
    (cloudHost) \
    (identityTime) \
    (aliveUpdateIntervalMs) \
    (protoVersion) \
    (connectionGuid)
NX_VMS_API_DECLARE_STRUCT(PeerDataEx)

} // namespace api
} // namespace vms
} // namespace nx
