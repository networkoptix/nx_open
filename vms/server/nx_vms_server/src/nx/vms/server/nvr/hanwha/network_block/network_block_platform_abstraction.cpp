#include "network_block_platform_abstraction.h"

#if defined (Q_OS_LINUX)
    #include <unistd.h>
    #include <sys/ioctl.h>
#endif

#include <optional>

#include <nx/vms/server/root_fs.h>

#if defined (Q_OS_LINUX)
    #include <nx/vms/server/nvr/hanwha/ioctl_common.h>
#endif

namespace nx::vms::server::nvr::hanwha {

static constexpr double kPoePlusPowerConsumptionLimitWatts = 30.0;
static constexpr int kFastEthernetLinkSpeedMbps = 100;
static const std::map<int, double> kPowerLimitWattsByDeviceClass = {
    {0, 15.4},
    {1, 4.5},
    {2, 7.0},
    {3, 15.4},
    {4, 30.0},
};

#if defined (Q_OS_LINUX)
static const QString kNetworkControllerDeviceFileName("/dev/ip1829_cdev");
static const QString kPowerSupplyDeviceFileName("/dev/poe");
static const std::map<int, int> kPortCountByBoardId = {
    {0x00, 16},
    {0x01, 8},
    {0x11, 4},
};

class NetworkBlockPlatformAbstractionImpl: public INetworkBlockPlatformAbstraction
{
public:
    NetworkBlockPlatformAbstractionImpl(RootFileSystem* rootFileSystem):
        m_rootFileSystem(rootFileSystem)
    {
        NX_INFO(this, "Opening the network controller device");
        m_networkControllerDeviceFd =
            m_rootFileSystem->open(kNetworkControllerDeviceFileName, QIODevice::ReadWrite);

        if (m_networkControllerDeviceFd < 0)
        {
            NX_ERROR(this,
                "Unable to open the network controller device %1",
                kNetworkControllerDeviceFileName);

            return;
        }

        NX_INFO(this, "Opening the power supply device");
        m_powerSupplyDeviceFd =
            m_rootFileSystem->open(kPowerSupplyDeviceFileName, QIODevice::ReadWrite);

        if (m_powerSupplyDeviceFd < 0)
        {
            NX_ERROR(this, "Unable to open the power supply device %1", kPowerSupplyDeviceFileName);
            return;
        }

        std::set<int> portNumbers;
        for (int i = 1; i < portCount(); ++i)
            portNumbers.insert(i);

        m_macAddressCache = getMacAddresses(std::move(portNumbers));
    }

    virtual ~NetworkBlockPlatformAbstractionImpl()
    {
        close(m_networkControllerDeviceFd);
        close(m_powerSupplyDeviceFd);
    }

    virtual int portCount() const override
    {
        NX_VERBOSE(this, "Getting port count");
        if (m_cachedPortCount)
            return *m_cachedPortCount;

        const int command = prepareCommand<int>(kGetBoardIdCommand);

        int boardId = -1;
        if (const int result = ::ioctl(m_powerSupplyDeviceFd, command, &boardId); result != 0)
        {
            NX_WARNING(this, "Unable to get board id, result %1", result);
            return 0;
        }

        if (const auto it = kPortCountByBoardId.find(boardId); it != kPortCountByBoardId.cend())
        {
            NX_VERBOSE(this, "Got port count for board id %1: %2", boardId, it->second);
            m_cachedPortCount = it->second;
            return it->second;
        }

        NX_WARNING(this, "Unknown board id: %1", boardId);
        return 0;
    }

    virtual NetworkPortState portState(int portNumber) const override
    {
        NX_ASSERT(portNumber > 0 && portNumber <= portCount());
        NX_VERBOSE(this, "Getting port state for port #%1", portNumber);
        NetworkPortState result;
        result.portNumber = portNumber;
        result.isPoeEnabled = isPoeEnabled(portNumber);

        if (!hasConnectedDevice(portNumber))
        {
            NX_VERBOSE(this, "No device is connected to port #%1", portNumber);
            m_macAddressCache[portNumber] = nx::utils::MacAddress();
            return result;
        }

        result.linkSpeedMbps = kFastEthernetLinkSpeedMbps;
        result.devicePowerConsumptionWatts = getPowerConsumptionWatts(portNumber);
        if (result.isPoeEnabled && !qFuzzyIsNull(result.devicePowerConsumptionWatts))
        {
            NX_VERBOSE(this, "Setting port #%1 power limit to %2",
                portNumber,
                kPoePlusPowerConsumptionLimitWatts);

            result.devicePowerConsumptionLimitWatts = kPoePlusPowerConsumptionLimitWatts;
        }

        result.macAddress = getConnectedDeviceMacAddress(portNumber);

        return result;
    }

    virtual bool setPoeEnabled(int portNumber, bool isPoeEnabled) override
    {
        NX_DEBUG(this, "%1 PoE on port #%2",
            (isPoeEnabled ? "Enabling": "Disabling"),
            portNumber);

        const int command =
            prepareCommand<PortPoweringStatusCommandData>(kSetPortPoweringStatusCommand);

        PortPoweringStatusCommandData commandData;
        commandData.portNumber = portNumber - 1;
        commandData.poweringStatus = isPoeEnabled;

        NX_DEBUG(this, "Set port powering mode command: %1, port #%2",
            command,
            portNumber);

        const int result = ::ioctl(m_powerSupplyDeviceFd, command, &commandData);
        if (result != 0)
        {
            NX_WARNING(this, "Unable to set port powering mode to %1, port #%2, error: %3",
                isPoeEnabled, portNumber, result);

            return false;
        }

        return true;
    }

private:
    std::map<int, nx::utils::MacAddress> getMacAddresses(std::set<int> portNumbers) const
    {
        std::map<int, nx::utils::MacAddress> macAddresses;
        const int command = prepareCommand<PortMacAddressCommandData>(kGetPortMacAddressCommand);
        for (int i = 0; i < 8192; ++i) //< TODO: #dmishin investigate what it is.
        {
            for (int j = 0; j < 2; ++j)
            {
                PortMacAddressCommandData commandData;
                commandData.index = i;
                commandData.block = j;

                const int result = ::ioctl(
                    m_networkControllerDeviceFd,
                    command,
                    &commandData);

                if (result == 0
                    && portNumbers.find(commandData.portNumber + 1) != portNumbers.cend())
                {
                    portNumbers.erase(commandData.portNumber);
                    nx::utils::MacAddress macAddress(nx::utils::MacAddress::Data{{
                        commandData.macAddress[0],
                        commandData.macAddress[1],
                        commandData.macAddress[2],
                        commandData.macAddress[3],
                        commandData.macAddress[4],
                        commandData.macAddress[5]}});

                    NX_VERBOSE(this, "Got MAC address %1 for port #%2, index %3, block %4",
                        macAddress, commandData.portNumber + 1, i, j);

                    macAddresses[commandData.portNumber + 1] = macAddress;

                    if (portNumbers.empty())
                        return macAddresses;
                }
            }
        }

        return macAddresses;
    }

    nx::utils::MacAddress getConnectedDeviceMacAddress(int portNumber) const
    {
        NX_VERBOSE(this, "Getting MAC address of the connected device on port %1", portNumber);
        if (const auto& cachedMacAddress = m_macAddressCache[portNumber];
            !cachedMacAddress.isNull())
        {
            NX_VERBOSE(this, "Got MAC address %1 from the cache for port %2",
                cachedMacAddress.toString(),
                portNumber);

            return cachedMacAddress;
        }

        std::map<int, nx::utils::MacAddress> result = getMacAddresses({portNumber});
        m_macAddressCache[portNumber] = result[portNumber];

        return result[portNumber];
    }

    double getPowerConsumptionWatts(int portNumber) const
    {
        const int command = prepareCommand<PortPowerConsumptionCommandData>(
            kGetPortPowerConsumptionCommand);

        PortPowerConsumptionCommandData commandData;
        commandData.portNumber = portNumber - 1;

#if 0
        if (const int result = ::ioctl(m_powerSupplyDeviceFd, command, &commandData); result != 0)
        {
            NX_WARNING(this, "Failed to get power consumption for port %1, error %2",
                portNumber, result);

            return 0.0;
        }
#else
        ::ioctl(m_powerSupplyDeviceFd, command, &commandData);
#endif

        const double voltage =
            (commandData.voltageMsb * 256 + commandData.voltageLsb) * 3.662 / 1000.0;

        const double current =
            (commandData.currentMsb * 256 + commandData.currentLsb) * 62.26 / 1000.0;

        const double powerConsumption = voltage * current / 1000.0;
        NX_VERBOSE(this,
            "Current power consumption on port %1: %2 watts", portNumber, powerConsumption);

        return powerConsumption;
    }

    bool hasConnectedDevice(int portNumber) const
    {
        const int command = prepareCommand<LinkStatusCommandData>(kGetPortLinkStatusCommand);

        LinkStatusCommandData commandData;
        commandData.portNumber = portNumber - 1;

        if (const int result = ::ioctl(m_networkControllerDeviceFd, command, &commandData);
            result != 0)
        {
            NX_WARNING(this, "Failed to obtain link status for port %1, error %2",
                portNumber, result);

            return false;
        }

        NX_VERBOSE(this, "Link status for port %1: %2", portNumber, commandData.linkStatus);
        return commandData.linkStatus != 0;
    }

    bool isPoeEnabled(int portNumber) const
    {
        const int command =
            prepareCommand<PortPoweringStatusCommandData>(kGetPortPoweringStatusCommand);

        PortPoweringStatusCommandData commandData;
        commandData.portNumber = portNumber - 1;

#if 0
        if (const int result = ::ioctl(m_powerSupplyDeviceFd, command, &commandData); result != 0)
        {
            NX_WARNING(this, "Failed to obtain powering status for port %1, error %2",
                portNumber, result);

            return false;
        }
#else
        ::ioctl(m_powerSupplyDeviceFd, command, &commandData);
#endif

        NX_VERBOSE(this,
            "Port powering status for port %1: %2", portNumber, commandData.poweringStatus);
        return commandData.poweringStatus != 0;
    }

private:
    RootFileSystem* const m_rootFileSystem = nullptr;

    int m_networkControllerDeviceFd = -1;
    int m_powerSupplyDeviceFd = -1;

    mutable std::map</*portNumber*/ int,  nx::utils::MacAddress> m_macAddressCache;
    mutable std::optional<int> m_cachedPortCount;
};

#else

class NetworkBlockPlatformAbstractionImpl: public INetworkBlockPlatformAbstraction
{
public:
    NetworkBlockPlatformAbstractionImpl(RootFileSystem* /*rootFileSystem*/) {}

    virtual int portCount() const override
    {
        NX_ASSERT(false, "Platform is not supported");
        return 0;
    }

    virtual NetworkPortState portState(int /*portNumber*/) const override
    {
        NX_ASSERT(false, "Platform is not supported");
        return {};
    }

    virtual bool setPoeEnabled(int portNumber, bool /*isPoeEnabled*/) override
    {
        NX_ASSERT(false, "Platform is not supported");
        return false;
    }
};

#endif

// ------------------------------------------------------------------------------------------------

NetworkBlockPlatformAbstraction::NetworkBlockPlatformAbstraction(RootFileSystem* rootFileSystem):
    m_impl(std::make_unique<NetworkBlockPlatformAbstractionImpl>(rootFileSystem))
{
}

NetworkBlockPlatformAbstraction::~NetworkBlockPlatformAbstraction()
{
    // Required for m_impl unique_ptr.
}

int NetworkBlockPlatformAbstraction::portCount() const
{
    return m_impl->portCount();
}

NetworkPortState NetworkBlockPlatformAbstraction::portState(int portNumber) const
{
    return m_impl->portState(portNumber);
}

bool NetworkBlockPlatformAbstraction::setPoeEnabled(int portNumber, bool isPoeEnabled)
{
    return m_impl->setPoeEnabled(portNumber, isPoeEnabled);
}

} // namespace nx::vms::server::nvr::hanwha
