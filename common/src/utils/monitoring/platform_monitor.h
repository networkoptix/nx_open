#ifndef QN_PLATFORM_MONITOR_H
#define QN_PLATFORM_MONITOR_H

#include <cstdint>

#include <QtCore/QString>

class QnPlatformMonitor {
public:
    enum SupportedFeature {
        TotalCpuUsage = 0x1,
        TotalRamUsage = 0x2,
    };
    Q_DECLARE_FLAGS(SupportedFeatures, SupportedFeature);

    /**
     * Description of an HDD.
     */
    struct Hdd {
        /** Hdd ID, to be used internally. */
        std::intptr_t id;

        /** Platform-specific string describing logical partitions of this HDD,
         * suitable to be shown to the user. */
        QString partitions;
    };

    virtual ~QnPlatformMonitor();

    /**
     * \returns                         Percent of CPU time (both user and kernel) consumed 
     *                                  by all running processes since the last call to this function,
     *                                  a number in range <tt>[0.0, 1.0]</tt>.
     */
    virtual qreal totalCpuUsage();

    /**
     * \returns                         Percent of RAM currently consumed by all running processes,
     *                                  a number in range <tt>[0.0, 1.0]</tt>.
     */
    virtual qreal totalRamUsage();

    /**
     * \returns                         A list of all HDDs on this PC.
     */
    virtual QList<Hdd> hdds();



private:
    Q_DISABLE_COPY(QnPlatformMonitor);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnPlatformMonitor::SupportedFeatures);

#endif // QN_PLATFORM_MONITOR_H
