#ifndef QN_PLATFORM_MONITOR_H
#define QN_PLATFORM_MONITOR_H

#if defined(_MSC_VER) && _MSC_VER<1600 
// TODO: msvc2008, remove this hell after transition to msvc2010
#   ifdef _WIN64
namespace std { typedef __int64 intptr_t; }
#   else
namespace std { typedef __int32 intptr_t; }
#   endif
#else
#   include <cstdint> /* For std::intptr_t. */
#endif

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QHash>

#include <utils/network/mac_address.h>

/**
 * Interface for monitoring performance in a platform-independent way.
 */
class QnPlatformMonitor: public QObject {
    Q_OBJECT
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
    };
    Q_DECLARE_FLAGS(PartitionTypes, PartitionType)

    /**
     * Partition space entry.
     */
    struct PartitionSpace {
        PartitionSpace(): type(UnknownPartition), freeBytes(0), sizeBytes(0) {}
        PartitionSpace(const QString &path, quint64 freeBytes, quint64 sizeBytes):
            path(path), type(UnknownPartition), freeBytes(freeBytes), sizeBytes(sizeBytes) {}

        /** Partition's root path. */
        QString path;

        /** Partition's type. */
        PartitionType type;

        /** Free space of this partition in bytes */
        quint64 freeBytes;

        /** Total size of this partition in bytes */
        quint64 sizeBytes;
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
        NetworkLoad(): type(UnknownInterface), bytesPerSecIn(0), bytesPerSecOut(0) {}

        /** Network interface name */
        QString interfaceName;

        /** Mac address. */
        QnMacAddress macAddress;

        /** Type of the network interface. */
        NetworkInterfaceType type;

        /** Current download speed in bytes per second */
        quint64 bytesPerSecIn;

        /** Current upload speed in bytes per second */
        quint64 bytesPerSecOut;
    };

    QnPlatformMonitor(QObject *parent = NULL): QObject(parent) {}
    virtual ~QnPlatformMonitor() {}

    /**
     * \param parent                    Parent object for the platform monitor to be created.
     * \returns                         A newly created platform monitor instance suitable for usage
     *                                  on current platform.
     */
    static QnPlatformMonitor *newInstance(QObject *parent = NULL);

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

Q_DECLARE_OPERATORS_FOR_FLAGS(QnPlatformMonitor::PartitionTypes);

#endif // QN_PLATFORM_MONITOR_H
