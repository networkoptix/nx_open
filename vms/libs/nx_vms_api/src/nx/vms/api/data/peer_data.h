// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>
#include <QtCore/QString>

#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/format.h>
#include <nx/vms/api/types/connection_types.h>

#include "id_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API PersistentIdData: IdData
{
    /** Unique persistent Db ID of the peer. Empty for clients. */
    nx::Uuid persistentId;

    PersistentIdData() = default;
    PersistentIdData(const nx::Uuid& id, const nx::Uuid& persistentId);

    bool operator==(const PersistentIdData& other) const = default;

    bool isNull() const { return id.isNull(); }

    bool operator<(const PersistentIdData& other) const;
    bool operator>(const PersistentIdData& other) const { return other < *this; }
};
#define PersistentIdData_Fields IdData_Fields(persistentId)
NX_VMS_API_DECLARE_STRUCT(PersistentIdData)
NX_REFLECTION_INSTRUMENT(PersistentIdData, PersistentIdData_Fields)

struct NX_VMS_API PeerData: PersistentIdData
{
    PeerData();

    PeerData(
        const nx::Uuid& id,
        const nx::Uuid& instanceId,
        PeerType peerType,
        Qn::SerializationFormat dataFormat = Qn::SerializationFormat::ubjson);

    PeerData(
        const PersistentIdData& peer,
        PeerType peerType,
        Qn::SerializationFormat dataFormat = Qn::SerializationFormat::ubjson);

    PeerData(
        const nx::Uuid& id,
        const nx::Uuid& instanceId,
        const nx::Uuid& persistentId,
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
    nx::Uuid instanceId;

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
#define PeerData_Fields PersistentIdData_Fields(instanceId)(peerType)(dataFormat)
NX_VMS_API_DECLARE_STRUCT(PeerData)
NX_REFLECTION_INSTRUMENT(PeerData, PeerData_Fields)

using PeerSet = QSet<nx::Uuid>;

struct PeerDataEx: public PeerData
{
    nx::Uuid systemId;
    QString cloudHost;
    qint64 identityTime = 0;
    int aliveUpdateIntervalMs = 0;
    int protoVersion = 0;
    nx::Uuid connectionGuid;

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
NX_REFLECTION_INSTRUMENT(PeerDataEx, PeerDataEx_Fields)

} // namespace api
} // namespace vms
} // namespace nx
