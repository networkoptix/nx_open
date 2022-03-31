// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>

#include "device_model.h"
#include "type_traits.h"

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

struct NX_VMS_API DeviceSearch
{
    /**%apidoc[readonly] Id of the search to get the status. */
    QnUuid id;

    /**%apidoc
     * IP address of the Device. Must be specified if startIp and endIp are not specified.
     * NOTE: Some Device drivers may also accept URL in this field.
     */
    std::optional<QString> ip;

    /**%apidoc
     * Starting IP address of the IP address range to search Device(s) in. Must be specified if
     * `ip` is not specified.
     */
    std::optional<QString> startIp;

    /**%apidoc
     * End IP of IP range to search Device(s) on. Must be specified if ip is not specified.
     */
    std::optional<QString> endIp;

    /**%apidoc Port to search device(s) on. */
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
    std::optional<std::vector<DeviceModelGeneral>> devices;

    QnUuid getId() const { return id; }
    void setId(QnUuid id) { this->id = std::move(id); }
    static_assert(nx::vms::api::isCreateModelV<DeviceSearch>);
};

#define DeviceSearch_Fields (id)(ip)(startIp)(endIp)(port)(credentials)(mode)(status)(devices)
QN_FUSION_DECLARE_FUNCTIONS(DeviceSearch, (json), NX_VMS_API)

} // namespace nx::vms::api
