// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QtCore/QFlags>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/mac_address.h>

namespace nx::monitoring {

/**
 * Interface for monitoring performance in a platform-independent way.
 */
class NX_MONITORING_API ActivityMonitor
{
public:
    /**
     * Description of an HDD.
     */
    struct Hdd
    {
        /** Hdd ID, to be used internally. */
        std::intptr_t id = -1;

        /** Platform-specific string specifying device name suitable to be
         * shown to the user. */
        std::string name;

        /** Platform-specific string describing logical partitions of this HDD,
         * suitable to be shown to the user. */
        std::string partitions;
    };

    /**
     * HDD load entry.
     */
    struct HddLoad
    {
        /** Description of an HDD. */
        Hdd hdd;

        /** Load percentage of the HDD since the last call to load estimation function,
         * a number in range <tt>[0.0, 1.0]</tt>. */
        double load = 0.0;
    };

    /**
     * Type of a partition.
     */
    enum class PartitionType: std::uint32_t
    {
        local = 0x01,
        ram = 0x02,
        optical = 0x04,
        swap = 0x08,
        network = 0x10,
        unknown = 0x20,
        removable = 0x40
    };

    template<typename Visitor>
    friend constexpr auto nxReflectVisitAllEnumItems(PartitionType*, Visitor&& visitor)
    {
        using Item = nx::reflect::enumeration::Item<PartitionType>;
        return visitor(
            Item{PartitionType::local, "local"},
            Item{PartitionType::ram, "ram"},
            Item{PartitionType::optical, "optical"},
            Item{PartitionType::swap, "swap"},
            Item{PartitionType::network, "network"},
            Item{PartitionType::removable, "removable"},
            Item{PartitionType::removable, "usb"}, //< Deprecated.
            Item{PartitionType::unknown, "unknown"});
    }

    Q_DECLARE_FLAGS(PartitionTypes, PartitionType)

    /**
     * Partition space entry.
     */
    struct PartitionSpace
    {
        PartitionSpace() = default;
        PartitionSpace(std::filesystem::path path, std::int64_t freeBytes, std::int64_t sizeBytes):
            path(std::move(path)), freeBytes(freeBytes), sizeBytes(sizeBytes)
        {
        }

        /** System-dependent name of device */
        std::string devName;

        /** Partition's root path. */
        std::filesystem::path path;

        /** Partition's type. */
        PartitionType type = PartitionType::unknown;

        /** Free space of this partition in bytes */
        std::int64_t freeBytes = 0;

        /** Total size of this partition in bytes */
        std::int64_t sizeBytes = 0;
    };

    enum NetworkInterfaceType
    {
        PhysicalInterface = 0x1,
        LoopbackInterface = 0x2,
        VirtualInterface = 0x4,
        UnknownInterface = 0x8
    };
    Q_DECLARE_FLAGS(NetworkInterfaceTypes, NetworkInterfaceType)

    /**
     * Network load entry
     */
    struct NetworkLoad
    {
        /** Network interface name */
        std::string interfaceName;

        /** Mac address. */
        nx::utils::MacAddress macAddress;

        /** Type of the network interface. */
        NetworkInterfaceType type = UnknownInterface;

        /** Current download speed in bytes per second */
        std::int64_t bytesPerSecIn = 0;

        /** Current upload speed in bytes per second */
        std::int64_t bytesPerSecOut = 0;

        /** Maximal transfer speed of the interface, in bytes per second. */
        std::int64_t bytesPerSecMax = 0;
    };

    ActivityMonitor() = default;
    virtual ~ActivityMonitor() = default;

    ActivityMonitor(const ActivityMonitor&) = delete;
    ActivityMonitor& operator=(const ActivityMonitor&) = delete;

    /**
     * @returns Percent of CPU time (both user and kernel) consumed by all running processes since
     *     the last call to this function, a number in range [0.0, 1.0].
     */
    virtual double totalCpuUsage() = 0;

    /**
     * @returns RAM currently consumed by all running processes, a number of bytes.
     */
    virtual std::uint64_t totalRamUsageBytes() = 0;

    /**
     * @returns Percent of CPU time (both user and kernel) consumed by the current process at
     * the moment, a number in range [0.0, 1.0].
     */
    virtual double thisProcessCpuUsage() = 0;

    /**
     * @returns Percent of GPU time consumed by the current process at the moment,
     * a number in range [0.0, 1.0].
     */
    virtual double thisProcessGpuUsage() { return 0.0; }

    /**
     * @returns RAM currently consumed by the current process, a number of bytes.
     */
    virtual std::uint64_t thisProcessRamUsageBytes() = 0;

    /**
     * @returns RAM currently consumed by private process memory, a number of bytes.
     */
    virtual std::uint64_t thisProcessPrivateRamUsageBytes() = 0;

    /**
     * @returns A list of HDD load entries for all HDDs on this PC.
     */
    virtual std::vector<HddLoad> totalHddLoad() = 0;

    /**
     * @returns A list of partition space entries for all partitions on this PC.
     */
    virtual std::vector<PartitionSpace> totalPartitionSpaceInfo() = 0;

    /**
     * @returns A list of network load entries for all network interfaces of the given types on
     * this PC.
     */
    virtual std::vector<NetworkLoad> totalNetworkLoad() = 0;

    /** @returns A total number of threads for the current process. Return 0 on error. */
    virtual int thisProcessThreads() = 0;

    /**
     * @returns A list of partition space entries for all partitions of the given types on this PC.
     */
    std::vector<PartitionSpace> totalPartitionSpaceInfo(PartitionTypes types);

    /**
     * @return Network load entry for the specified network interface on this PC.
     */
    NetworkLoad networkInterfaceLoadOrThrow(std::string_view interfaceName);
    NetworkLoad networkInterfaceLoadOrThrow(const nx::utils::MacAddress& macAddress);

    /**
     * @returns A list of network load entries for all network interfaces of the given types on
     * this PC.
     */
    std::vector<NetworkLoad> totalNetworkLoad(NetworkInterfaceTypes types);

    /** @returns Server uptime in milliseconds. */
    virtual std::chrono::milliseconds processUptime() const { return std::chrono::milliseconds(0); }

    /** @returns Update period of values, in milliseconds. */
    virtual std::chrono::milliseconds updatePeriod() const { return std::chrono::milliseconds(0); }

    class PartitionsInformationProvider
    {
    public:
        virtual ~PartitionsInformationProvider() = default;

        virtual std::vector<PartitionSpace> partitionInfo() const = 0;
    };

    virtual void setPartitionInformationProvider(
        [[maybe_unused]] std::unique_ptr<PartitionsInformationProvider> partitionInformationProvider)
    {}

    virtual void logStatistics() {}

    /** Create platform specific implementation. */
    static std::unique_ptr<ActivityMonitor> createForCurrentPlatform();

    /** Determine partition type by its filesystem type name. */
    static PartitionType getPartitionTypeByFsType(std::string_view fsTypeName);
};

NX_MONITORING_API std::string toString(const ActivityMonitor::PartitionSpace& value);

} // namespace nx::monitoring

Q_DECLARE_OPERATORS_FOR_FLAGS(nx::monitoring::ActivityMonitor::PartitionTypes)
Q_DECLARE_OPERATORS_FOR_FLAGS(nx::monitoring::ActivityMonitor::NetworkInterfaceTypes)
