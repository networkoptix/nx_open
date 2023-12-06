// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx::vms::api {

struct NX_VMS_API NetworkPortWithPoweringMode
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(PoweringMode,
        /**%apidoc
         * PoE is forcibly disabled on the port.
         */
        off,

        /**%apidoc
         * PoE is forcibly enabled on the port.
         */
        on,

        /**%apidoc
         * OS driver is managing the power supply on the port.
         */
        automatic
    )

    /**%apidoc
     * Number of the port, starting from 1.
     * %example 6000
     */
    int portNumber = 0;

    /**%apidoc
     * Device power supply policy defined by the user.
     */
    PoweringMode poweringMode = PoweringMode::off;

    bool operator==(const NetworkPortWithPoweringMode& other) const = default;
};

#define NetworkPortWithPoweringMode_Fields \
    (portNumber) \
    (poweringMode)

NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(NetworkPortWithPoweringMode, (json))

struct NX_VMS_API NetworkPortState: public NetworkPortWithPoweringMode
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(PoweringStatus,
        /**%apidoc
         * Device is disconnected, no power supply, no networking at all.
         */
        disconnected,

        /**%apidoc
         * Device is connected but doesn't require power supply.
         */
        connected,

        /**%apidoc
         * Device is connected and is being powered.
         */
        powered
    )

    /**%apidoc
     * MAC address of the connected device.
     */
    QString macAddress;

    /**%apidoc:float
     * Current real device power consumption in watts.
     */
    double devicePowerConsumptionWatts = 0.0;

    /**%apidoc:float
     * Device power consumption limit depending on its PoE class.
     */
    double devicePowerConsumptionLimitWatts = 0.0;

    /**%apidoc
     * Link speed in Mbps.
     */
    int linkSpeedMbps = 0;

    /**%apidoc
     * Current device power supply status.
     */
    PoweringStatus poweringStatus = PoweringStatus::disconnected;

    bool operator==(const NetworkPortState& other) const;
};

#define NetworkPortState_Fields \
    (portNumber) \
    (poweringMode) \
    (macAddress) \
    (devicePowerConsumptionWatts) \
    (devicePowerConsumptionLimitWatts) \
    (linkSpeedMbps) \
    (poweringStatus)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(NetworkPortState, (json))
NX_REFLECTION_INSTRUMENT(NetworkPortState, NetworkPortState_Fields)

/**%apidoc
 * Information about NVR network block, including each port state and the total power consumption
 * limit.
 */
struct NX_VMS_API NetworkBlockData
{
    /**%apidoc Current network port states. */
    NetworkPortStateList portStates;

    /**%apidoc:float
     * Upper power limit for the NVR in watts. When this limit is exceeded NVR goes to the "PoE
     *     over budget" state.
     */
    double upperPowerLimitWatts = 0.0;

    /**%apidoc:float
     * Lower power limit for the NVR in watts. If the NVR is in the "PoE over budget" state and
     *     power consumption goes below this limit then the NVR goes to the normal state.
     */
    double lowerPowerLimitWatts = 0.0;

    /**%apidoc
     * Indicates whether the NVR is in "PoE over budget" state.
     */
    bool isInPoeOverBudgetMode = false;

    bool operator==(const NetworkBlockData& other) const;
};
#define NetworkBlockData_Fields \
    (upperPowerLimitWatts) \
    (lowerPowerLimitWatts) \
    (isInPoeOverBudgetMode) \
    (portStates)

NX_VMS_API_DECLARE_STRUCT_EX(NetworkBlockData, (json))
NX_REFLECTION_INSTRUMENT(NetworkBlockData, NetworkBlockData_Fields)

struct NX_VMS_API NvrNetworkRequest
{
    QnUuid getId() { return id; }

    /**%apidoc:string
     * Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers` or be "this" to
     * refer to the current Server.
     * %example this
     */
    QnUuid id;

    NetworkPortWithPoweringModeList portPowerList;
};

#define NvrNetworkRequest_Fields (id)(portPowerList)

NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(NvrNetworkRequest, (json))
NX_REFLECTION_INSTRUMENT(NvrNetworkRequest, NvrNetworkRequest_Fields)

} // namespace nx::vms::api
