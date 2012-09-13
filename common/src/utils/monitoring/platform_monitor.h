#ifndef QN_PLATFORM_MONITOR_H
#define QN_PLATFORM_MONITOR_H

#include <cstdint> /* For std::intptr_t. */

#include <QtCore/QObject>
#include <QtCore/QString>

/**
 * Interface for monitoring performance in a platform-independent way.
 */
class QnPlatformMonitor: public QObject {
    Q_OBJECT;
public:
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

    QnPlatformMonitor(QObject *parent = NULL): QObject(parent) {}
    virtual ~QnPlatformMonitor() {}

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
     * \returns                         A list of all HDDs on this PC.
     */
    virtual QList<Hdd> hdds() = 0;

    /**
     * \param hdd                       HDD to get load information for.
     * \returns                         Load percentage of the given HDD since the last call to this function,
     *                                  a number in range <tt>[0.0, 1.0]</tt>.
     */
    virtual qreal totalHddLoad(const Hdd &hdd) = 0;

private:
    Q_DISABLE_COPY(QnPlatformMonitor);
};

#endif // QN_PLATFORM_MONITOR_H
