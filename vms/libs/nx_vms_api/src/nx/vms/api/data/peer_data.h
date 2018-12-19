#pragma once

#include "id_data.h"

#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <nx/fusion/serialization_format.h>
#include <nx/vms/api/types_fwd.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API PersistentIdData: IdData
{
    /** Unique persistent Db ID of the peer. Empty for clients. */
    QnUuid persistentId;

    PersistentIdData() = default;
    PersistentIdData(const QnUuid& id, const QnUuid& persistentId);

    bool isNull() const { return id.isNull(); }

    bool operator<(const PersistentIdData& other) const;
    bool operator>(const PersistentIdData& other) const { return other < *this; }
};
#define PersistentIdData_Fields \
    IdData_Fields \
    (persistentId)

struct NX_VMS_API PeerData: PersistentIdData
{
    PeerData();

    PeerData(
        const QnUuid& id,
        const QnUuid& instanceId,
        PeerType peerType,
        Qn::SerializationFormat dataFormat = Qn::UbjsonFormat);

    PeerData(
        const PersistentIdData& peer,
        PeerType peerType,
        Qn::SerializationFormat dataFormat = Qn::UbjsonFormat);

    PeerData(
        const QnUuid& id,
        const QnUuid& instanceId,
        const QnUuid& persistentId,
        PeerType peerType,
        Qn::SerializationFormat dataFormat = Qn::UbjsonFormat);

    bool isClient() const { return isClient(peerType); }
    bool isServer() const { return isServer(peerType); }
    bool isCloudServer() const { return isCloudServer(peerType); }
    bool isMobileClient() const { return isMobileClient(peerType); }

    static bool isServer(PeerType peerType);
    static bool isClient(PeerType peerType);
    static bool isMobileClient(PeerType peerType);
    static bool isCloudServer(PeerType peerType);

    /** Unique running instance ID of the peer. */
    QnUuid instanceId;

    /** Type of the peer. */
    PeerType peerType;

    /** Preferred client data serialization format */
    Qn::SerializationFormat dataFormat = Qn::UbjsonFormat;
};
#define PeerData_Fields \
    PersistentIdData_Fields \
    (instanceId) \
    (peerType) \
    (dataFormat)

using PeerSet = QSet<QnUuid>;

struct PeerDataEx: public PeerData
{
    QnUuid systemId;
    QString cloudHost;
    qint64 identityTime = 0;
    int aliveUpdateIntervalMs = 0;
    int protoVersion = 0;

    void assign(const PeerData& data) { PeerData::operator=(data); }
};

#define PeerDataEx_Fields \
    PeerData_Fields \
    (systemId) \
    (cloudHost) \
    (identityTime) \
    (aliveUpdateIntervalMs) \
    (protoVersion)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::PersistentIdData)
Q_DECLARE_METATYPE(nx::vms::api::PeerData)
Q_DECLARE_METATYPE(nx::vms::api::PeerDataEx)
