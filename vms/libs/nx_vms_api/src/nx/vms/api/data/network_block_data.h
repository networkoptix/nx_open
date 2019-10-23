#pragma once

#include <map>

#include <nx/utils/uuid.h>
#include <nx/vms/api/data/data.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api {

struct NX_VMS_API NetworkPortWithPoweringMode: public Data
{
    enum class PoweringMode
    {
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
        automatic,
    };

    /**%apidoc
     * Number of the port, starting from 1.
     */
    int portNumber = 0;

    /**%apidoc
     * Device power supply policy defined by the user.
     */
    PoweringMode poweringMode = PoweringMode::off;
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(NetworkPortWithPoweringMode::PoweringMode);

#define nx_vms_api_NetworkPortWithPoweringMode_Fields \
    (portNumber) \
    (poweringMode)

QN_FUSION_DECLARE_FUNCTIONS(NetworkPortWithPoweringMode, (json), NX_VMS_API);

struct NX_VMS_API NetworkPortState: public NetworkPortWithPoweringMode
{
    enum class PoweringStatus
    {
        /**%apidoc
         * Device is disconnected, no power supply, no networking at all.
         */
        disconnected,

        /**%apidoc
         * Device is connected bu doesn't require power supply.
         */
        connected,

        /**%apidoc
         * Device is connected and is being powered.
         */
        powered,
    };

    /**%apidoc
     * Id of the connected device (if any), zero UUID otherwise.
     */
    QnUuid deviceId;

    /**%apidoc
     * Current real device power consumption in watts.
     */
    double devicePowerConsumptionWatts = 0.0;

    /**%apidoc
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
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(NetworkPortState::PoweringStatus);

#define nx_vms_api_NetworkPortState_Fields \
    (deviceId) \
    (devicePowerConsumptionWatts) \
    (devicePowerConsumptionLimitWatts) \
    (linkSpeedMbps) \
    (poweringStatus)

QN_FUSION_DECLARE_FUNCTIONS(NetworkPortState, (json), NX_VMS_API);

struct NX_VMS_API NetworkBlockData
{
    /**%apidoc
     * Array of current network port states with the folowing structure:
     *     %struct NetworkPortState
     */
    std::vector<NetworkPortState> portStates;

    /**%apidoc
     * Overall power limit for NVR in watts.
     */
    double totalPowerLimitWatts = 0.0;
};
#define nx_vms_api_NetworkBlockData_Fields \
    (portStates) \
    (totalPowerLimitWatts)

QN_FUSION_DECLARE_FUNCTIONS(NetworkBlockData, (json), NX_VMS_API);

} // namespace nx::vms::api

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::NetworkPortWithPoweringMode::PoweringMode,
    (lexical), NX_VMS_API);

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::NetworkPortState::PoweringStatus, (lexical), NX_VMS_API);

