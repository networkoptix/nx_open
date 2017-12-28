#ifndef QN_PLATFORM_MONITOR_H
#define QN_PLATFORM_MONITOR_H

#include <cstdint> /* For std::intptr_t. */

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QHash>

#include <nx/network/mac_address.h>
#include <nx/fusion/model_functions_fwd.h>


/**
 * Interface for monitoring performance in a platform-independent way.
 */
class QnPlatformMonitor: public QObject {
    Q_OBJECT
    Q_FLAGS(PartitionTypes NetworkInterfaceTypes)

public:
    /**
     * Description of an HDD.
     */
    struct Hdd {
        Hdd(): id(-1) {}
        Hdd(std::intptr_t id, const QString &name, const QString &partitions): id(id), name(name), partitions(partitions) {}

        /** Hdd ID, to be used internally. */
        std::intptr_t id;

        /** Platform-specific string specifying device name suitable to be
         * shown to the user. */
        QString name;

        /** Platform-specific string describing logical partitions of this HDD,
         * suitable to be shown to the user. */
        QString partitions;

        friend uint qHash(const Hdd &hdd) {
            return qHash(hdd.id) ^ qHash(hdd.name) ^ qHash(hdd.partitions);
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
    enum PartitionType {
        LocalDiskPartition      = 0x01,
        RamDiskPartition        = 0x02,
        OpticalDiskPartition    = 0x04,
        SwapPartition           = 0x08,
        NetworkPartition        = 0x10,
        UnknownPartition        = 0x20,
        RemovableDiskPartition  = 0x40
    };
    Q_DECLARE_FLAGS(PartitionTypes, PartitionType)

    /**
     * Partition space entry.
     */
    struct PartitionSpace {
        PartitionSpace(): type(UnknownPartition), freeBytes(0), sizeBytes(0) {}
        PartitionSpace(const QString &path, qint64 freeBytes, qint64 sizeBytes):
            path(path), type(UnknownPartition), freeBytes(freeBytes), sizeBytes(sizeBytes) {}

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
        NetworkLoad(): type(UnknownInterface), bytesPerSecIn(0), bytesPerSecOut(0), bytesPerSecMax(0) {}

        /** Network interface name */
        QString interfaceName;

        /** Mac address. */
        nx::network::QnMacAddress macAddress;

        /** Type of the network interface. */
        NetworkInterfaceType type;

        /** Current download speed in bytes per second */
        qint64 bytesPerSecIn;

        /** Current upload speed in bytes per second */
        qint64 bytesPerSecOut;

        /** Maximal transfer speed of the interface, in bytes per second. */
        qint64 bytesPerSecMax;
    };

    QnPlatformMonitor(QObject *parent = NULL): QObject(parent) {}
    virtual ~QnPlatformMonitor() {}

    // TODO: #Elric remove 'total' from names.

    /**
     * \returns                         Percent of CPU time (both user and kernel) consumed
     *                                  by all running processes since the last call to this function,
     *                                  a number in range <tt>[0.0, 1.0]</tt>.
     */
    virtual qreal totalCpuUsage() = 0;

    /**
     * \returns                         Percent of RAM currently consumed by all running processes,
     *                                  a number in range <tt>[0.0, 1.0]</tt>.
     */
    virtual qreal totalRamUsage() = 0;

    /**
     * \returns                         A list of HDD load entries for all HDDs on this PC.
     */
    virtual QList<HddLoad> totalHddLoad() = 0;

    /**
     * \returns                         A list of network load entries for all network interfaces on this PC.
     */
    virtual QList<NetworkLoad> totalNetworkLoad() = 0;

    /**
     * @returns                         A list of network load entries for all network interfaces of the given types on this PC.
     */
    QList<NetworkLoad> totalNetworkLoad(NetworkInterfaceTypes types);

    /**
     * @returns                         A list of partition space entries for all partitions on this PC.
     */
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() = 0;

    /**
     * @returns                         A list of partition space entries for all partitions of the given types on this PC.
     */
    QList<PartitionSpace> totalPartitionSpaceInfo(PartitionTypes types);

    /**
     * @brief partitionByPath           Get partition name by path to some folder located on this partition.
     *                                  Used to get partition by path to the storage.
     * @param path                      Platform-specific path to target folder.
     * @returns                         Platform-specific string describing this logical partition,
     *                                  suitable to be shown to the user.
     */
    virtual QString partitionByPath(const QString &path) { return QString(); Q_UNUSED(path); }

private:
    Q_DISABLE_COPY(QnPlatformMonitor)
};

QN_FUSION_DECLARE_FUNCTIONS(QnPlatformMonitor::PartitionType, (metatype)(lexical));
QN_FUSION_DECLARE_FUNCTIONS(QnPlatformMonitor::PartitionTypes, (metatype)(lexical));
Q_DECLARE_OPERATORS_FOR_FLAGS(QnPlatformMonitor::PartitionTypes);

QString toString(const QnPlatformMonitor::PartitionSpace& value);

#endif // QN_PLATFORM_MONITOR_H
