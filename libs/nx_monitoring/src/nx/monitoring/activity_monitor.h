// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>

#include <QtCore/QObject>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/mac_address.h>
#include <nx/utils/serialization/flags.h>

namespace nx::monitoring {

/**
 * Interface for monitoring performance in a platform-independent way.
 */
class NX_MONITORING_API ActivityMonitor: public QObject
{
public:
    /**
     * Description of an HDD.
     */
    struct Hdd {
        Hdd(): id(-1) {}
        Hdd(std::intptr_t id, const QString &name, const QString &partitions):
            id(id), name(name), partitions(partitions) {}

        /** Hdd ID, to be used internally. */
        std::intptr_t id;

        /** Platform-specific string specifying device name suitable to be
         * shown to the user. */
        QString name;

        /** Platform-specific string describing logical partitions of this HDD,
         * suitable to be shown to the user. */
        QString partitions;

        friend size_t qHash(const Hdd& hdd) {
            return ::qHash(hdd.id) ^ qHash(hdd.name) ^ qHash(hdd.partitions);
        }
    };

    /**
     * HDD load entry.
     */
    struct HddLoad {
        HddLoad() {}
        HddLoad(const Hdd &hdd, qreal load): hdd(hdd), load(load) {}

        /** Description of an HDD. */
        Hdd hdd;

        /** Load percentage of the HDD since the last call to load estimation function,
         * a number in range <tt>[0.0, 1.0]</tt>. */
        qreal load;
    };

    /**
     * Type of a partition.
     */
    enum class PartitionType
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
    struct PartitionSpace {
        PartitionSpace(): type(PartitionType::unknown), freeBytes(0), sizeBytes(0) {}
        PartitionSpace(const QString &path, qint64 freeBytes, qint64 sizeBytes):
            path(path), type(PartitionType::unknown), freeBytes(freeBytes), sizeBytes(sizeBytes) {}

        /** System-dependent name of device */
        QString devName;

        /** Partition's root path. */
        QString path;

        /** Partition's type. */
        PartitionType type;

        /** Free space of this partition in bytes */
        qint64 freeBytes;

        /** Total size of this partition in bytes */
        qint64 sizeBytes;
    };

    enum NetworkInterfaceType {
        PhysicalInterface = 0x1,
        LoopbackInterface = 0x2,
        VirtualInterface  = 0x4,
        UnknownInterface  = 0x8
    };
    Q_DECLARE_FLAGS(NetworkInterfaceTypes, NetworkInterfaceType)

    /**
     * Network load entry
     */
    struct NetworkLoad {
        NetworkLoad():
            type(UnknownInterface), bytesPerSecIn(0), bytesPerSecOut(0), bytesPerSecMax(0) {}

        /** Network interface name */
        QString interfaceName;

        /** Mac address. */
        nx::utils::MacAddress macAddress;

        /** Type of the network interface. */
        NetworkInterfaceType type;

        /** Current download speed in bytes per second */
        qint64 bytesPerSecIn;

        /** Current upload speed in bytes per second */
        qint64 bytesPerSecOut;

        /** Maximal transfer speed of the interface, in bytes per second. */
        qint64 bytesPerSecMax;
    };

    ActivityMonitor() {}
    virtual ~ActivityMonitor() = default;

    /**
     * @returns Percent of CPU time (both user and kernel) consumed by all running processes since
     *     the last call to this function, a number in range [0.0, 1.0].
     */
    virtual qreal totalCpuUsage() = 0;

    /**
     * @returns RAM currently consumed by all running processes, a number of bytes.
     */
    virtual quint64 totalRamUsageBytes() = 0;

    /**
     * @returns Percent of CPU time (both user and kernel) consumed by the current process at
     * the moment, a number in range [0.0, 1.0].
     */
    virtual qreal thisProcessCpuUsage() = 0;

    /**
     * @returns Percent of GPU time consumed by the current process at the moment,
     * a number in range [0.0, 1.0].
     */
    virtual qreal thisProcessGpuUsage() { return 0.0; }

    /**
     * @returns RAM currently consumed by the current process, a number of bytes.
     */
    virtual quint64 thisProcessRamUsageBytes() = 0;

    /**
     * @returns RAM currently consumed by private process memory, a number of bytes.
     */
    virtual quint64 thisProcessPrivateRamUsageBytes() = 0;

    /**
     * @returns A list of HDD load entries for all HDDs on this PC.
     */
    virtual QList<HddLoad> totalHddLoad() = 0;

    /**
     * @returns A list of partition space entries for all partitions on this PC.
     */
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() = 0;

    /**
     * @returns A list of network load entries for all network interfaces of the given types on
     * this PC.
     */
    virtual QList<NetworkLoad> totalNetworkLoad() = 0;

    /** @returns A total number of threads for the current process. Return 0 on error. */
    virtual int thisProcessThreads() = 0;

    /**
     * @returns A list of partition space entries for all partitions of the given types on this PC.
     */
    QList<PartitionSpace> totalPartitionSpaceInfo(PartitionTypes types);

    /**
     * @return Network load entry for the specified network interface on this PC.
     */
    NetworkLoad networkInterfaceLoadOrThrow(const QString& interfaceName);
    NetworkLoad networkInterfaceLoadOrThrow(const nx::utils::MacAddress& macAddress);

    /**
     * @returns A list of network load entries for all network interfaces of the given types on
     * this PC.
     */
    QList<NetworkLoad> totalNetworkLoad(NetworkInterfaceTypes types);

    /** @returns Server uptime in milliseconds. */
    virtual std::chrono::milliseconds processUptime() const { return std::chrono::milliseconds(0); }

    /** @returns Update period of values, in milliseconds. */
    virtual std::chrono::milliseconds updatePeriod() const { return std::chrono::milliseconds(0); }

    class PartitionsInformationProvider
    {
    public:
        virtual ~PartitionsInformationProvider() = default;

        virtual QList<PartitionSpace> partitionInfo() const = 0;
    };

    virtual void setPartitionInformationProvider(
        [[maybe_unused]] std::unique_ptr<PartitionsInformationProvider> partitionInformationProvider)
    {}

    virtual void logStatistics() {}

    /** Create platform specific implementation. */
    static std::unique_ptr<ActivityMonitor> createForCurrentPlatform();

    /** Determine partition type by its filesystem type name. */
    static PartitionType getPartitionTypeByFsType(const QString& fsTypeName);

private:
    Q_DISABLE_COPY(ActivityMonitor)
};

NX_MONITORING_API QString toString(const ActivityMonitor::PartitionSpace& value);

} // namespace nx::monitoring

Q_DECLARE_OPERATORS_FOR_FLAGS(nx::monitoring::ActivityMonitor::PartitionTypes)
