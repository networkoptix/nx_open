// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/type_traits.h>

#include "device_model.h"

namespace nx::vms::api {

/**%apidoc State of the running manual Device search process. */
struct NX_VMS_API DeviceSearchStatus
{
    NX_REFLECTION_ENUM_IN_CLASS(State,
        Init,
        CheckingOnline,
        CheckingHost,
        Finished,
        Aborted
    )

    DeviceSearchStatus() {}
    DeviceSearchStatus(State state, quint64 current, quint64 total):
        state(state), current(current), total(total)
    {
    }

    /**%apidoc[opt] Current state of the process. */
    State state = Aborted;

    /**%apidoc[opt] Index of currently processed element. */
    qint64 current = 0;

    /**%apidoc[opt] Number of elements on the current stage. */
    qint64 total = 0;
};

#define DeviceSearchStatus_Fields (state)(current)(total)
QN_FUSION_DECLARE_FUNCTIONS(DeviceSearchStatus, (csv_record)(json)(ubjson)(xml), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceSearchStatus, DeviceSearchStatus_Fields)

struct NX_VMS_API DeviceSearchId
{
    /**%apidoc[readonly] Id of the search to get the status. */
    QString id;

    DeviceSearchId(QString id = {}): id(std::move(id)) {}
    nx::Uuid searchId() const;
    nx::Uuid serverIdFromId() const;

    void setIds(const nx::Uuid& searchId, const nx::Uuid& serverId);
    const QString& toString() const { return id; }
};
#define DeviceSearchId_Fields (id)
QN_FUSION_DECLARE_FUNCTIONS(DeviceSearchId, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceSearchId, DeviceSearchId_Fields)

struct NX_VMS_API DeviceSearchBase: DeviceSearchId
{
    /**%apidoc Target port to search Device(s) on. */
    std::optional<int> port;

    /**%apidoc Device credentials if necessary. */
    std::optional<Credentials> credentials;

    /**%apidoc
     * If specified, the handler hangs until the search is completed. WARNING: This may take a
     * significant amount of time. `waitResults` just waits for the search and `addFoundDevices`
     * additionally adds found Devices the same way `POST /rest/v{1-}/devices` does. The search is
     * removed like after the DELETE call.
     */
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Mode,
        waitResults,
        addFoundDevices
    )
    std::optional<Mode> mode;

    /**%apidoc Status of search request. */
    std::optional<DeviceSearchStatus> status;

    /**%apidoc Already found Devices. */
    std::optional<std::vector<DeviceModelForSearch>> devices;

    /**%apidoc[opt]
     * Id of the Server to perform search. The current Server is used
     * if serverId is not specified.
     */
    nx::Uuid serverId;

    void stripIdToLocal();
};
#define DeviceSearchBase_Fields DeviceSearchId_Fields(port)(credentials)(mode)(status)(devices)(serverId)

struct NX_VMS_API DeviceSearchIp
{
    /**%apidoc
     * IP address of the Device. NOTE: Some Device drivers can also accept URL in this field.
     * %example 192.168.0.1
     */
    QString ip;
};
#define DeviceSearchIp_Fields (ip)
QN_FUSION_DECLARE_FUNCTIONS(DeviceSearchIp, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceSearchIp, DeviceSearchIp_Fields)

struct NX_VMS_API DeviceSearchIpRange
{
    /**%apidoc
     * Start IP address of the IP address range to search Device(s) in.
     * %example 192.168.0.1
     */
    QString startIp;

    /**%apidoc
     * End IP address of the IP address range to search Device(s) in.
     * %example 192.168.0.255
     */
    QString endIp;
};
#define DeviceSearchIpRange_Fields (startIp)(endIp)
QN_FUSION_DECLARE_FUNCTIONS(DeviceSearchIpRange, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceSearchIpRange, DeviceSearchIpRange_Fields)

struct NX_VMS_API DeviceSearchV2: DeviceSearchBase
{
    std::variant<DeviceSearchIp, DeviceSearchIpRange> target;
};
#define DeviceSearchV2_Fields DeviceSearchBase_Fields(target)
QN_FUSION_DECLARE_FUNCTIONS(DeviceSearchV2, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceSearchV2, DeviceSearchV2_Fields)

struct NX_VMS_API DeviceSearchV1: DeviceSearchBase
{
    /**%apidoc
     * IP address of the Device. Must be specified if `startIp` and `endIp` are not specified.
     * NOTE: Some Device drivers can also accept URL in this field.
     */
    std::optional<QString> ip;

    /**%apidoc
     * Start IP address of the IP address range to search Device(s) in. Must be specified if `ip`
     * is not specified.
     */
    std::optional<QString> startIp;

    /**%apidoc
     * End IP address of the IP address range to search Device(s) in. Must be specified if `ip` is
     * not specified.
     */
    std::optional<QString> endIp;

    DeviceSearchV1() = default;
    DeviceSearchV1(DeviceSearchV2&& origin);
};
#define DeviceSearchV1_Fields DeviceSearchBase_Fields(ip)(startIp)(endIp)
QN_FUSION_DECLARE_FUNCTIONS(DeviceSearchV1, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceSearchV1, DeviceSearchV1_Fields)

using DeviceSearch = DeviceSearchV2;

} // namespace nx::vms::api
