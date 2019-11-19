#pragma once

#include <vector>
#include <functional>

#include <nx/utils/mac_address.h>
#include <api/model/api_ioport_data.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::server::nvr {

struct NetworkPortPoeState
{
    int portNumber = 0;
    bool isPoeEnabled = false;

    QString toString() const
    {
        return lm("{portNumber: %1; isPoeEnabled: %2}").args(portNumber, isPoeEnabled);
    }
};

struct NetworkPortState: public NetworkPortPoeState
{
    nx::utils::MacAddress macAddress;
    double devicePowerConsumptionWatts = 0.0;
    double devicePowerConsumptionLimitWatts = 0.0;
    int linkSpeedMbps = 0;

    QString toString() const
    {
        return lm("{portNumber: %1; isPoeEnabled: %2, macAddress: %3; "
            "powerConsumption: %4 watts; powerConsumptionLimit: %5 watts; linkSpeed: %6 Mbps}")
            .args(portNumber, isPoeEnabled, macAddress,
                devicePowerConsumptionWatts, devicePowerConsumptionLimitWatts, linkSpeedMbps);
    }
};

using NetworkPortPoeStateList = std::vector<NetworkPortPoeState>;
using NetworkPortStateList = std::vector<NetworkPortState>;

inline bool operator==(const NetworkPortPoeState& rhs, const NetworkPortPoeState& lhs)
{
    return rhs.portNumber == lhs.portNumber && rhs.isPoeEnabled == lhs.isPoeEnabled;
}

using IoStateChangeHandler = std::function<void(const QnIOStateDataList& state)>;

struct DeviceInfo
{
    QString vendor;
    QString model;
};

enum class FanState
{
    undefined,
    ok,
    error,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(FanState);

QString toString(FanState state);

enum class BuzzerState
{
    undefined,
    disabled,
    enabled,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(BuzzerState);

QString toString(BuzzerState state);

enum class IoPortState
{
    undefined,
    inactive,
    active,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(IoPortState);

QString toString(IoPortState state);

enum class LedState
{
    undefined,
    disabled,
    enabled,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(LedState);

QString toString(LedState state);

struct LedDescription
{
    QString id;
    QString name;
};

} // namespace nx::vms::server::nvr

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::server::nvr::FanState, (lexical))
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::server::nvr::BuzzerState, (lexical))
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::server::nvr::IoPortState, (lexical))
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::server::nvr::LedState, (lexical))
