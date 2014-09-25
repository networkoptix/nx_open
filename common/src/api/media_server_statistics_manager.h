#ifndef QN_STATISTICS_MANAGER
#define QN_STATISTICS_MANAGER

#include <QtCore/QString>
#include <QtCore/QHash>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <api/model/statistics_reply.h>

class QnMediaServerStatisticsStorage;

/**
  * Class that receives, parses and stores statistics data from all servers.
  * Also handles request sending through an inner timer.
  */
class QnMediaServerStatisticsManager: public QObject {
    Q_OBJECT
public:
    /**
     * Constructor
     *
     * \param parent            Parent of the object
     */
    QnMediaServerStatisticsManager(QObject *parent = NULL);

    /**
     *  Register the consumer object.
     *
     * \param resource          Server resource whose history we want to receive.
     * \param target            Object that will be notified about new data.
     * \param slot              Slot that will be called when new data will be received.
     */
    void registerConsumer(const QnMediaServerResourcePtr &resource, QObject *target, const char *slot);

    /**
     *  Unregister the consumer object.
     *
     * \param resource          Server resource whose history we do not want to receive anymore.
     * \param target            Object that will not be notified about new data anymore.
     */
    void unregisterConsumer(const QnMediaServerResourcePtr &resource, QObject *target);

    QnStatisticsHistory history(const QnMediaServerResourcePtr &resource) const;
    qint64 uptimeMs(const QnMediaServerResourcePtr &resource) const;
    qint64 historyId(const QnMediaServerResourcePtr &resource) const;

    /** Data update period in milliseconds. It is taken from the server's response. */
    int updatePeriod(const QnMediaServerResourcePtr &resource) const;

    /** Number of data points that are stored simultaneously. */
    int pointsLimit() const;

    /** Filter statistics items of some deviceType by flags (ignore all replies that do not contain flags provided). */
    void setFlagsFilter(Qn::StatisticsDeviceType deviceType, int flags);

private:
    QHash<QnUuid, QnMediaServerStatisticsStorage *> m_statistics;
    QHash<Qn::StatisticsDeviceType, int> m_flagsFilter;
};

#endif // QN_STATISTICS_MANAGER
