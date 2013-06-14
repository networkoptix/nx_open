#ifndef QN_STATISTICS_MANAGER
#define QN_STATISTICS_MANAGER

#include <QtCore/QString>
#include <QtCore/QHash>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <api/model/statistics_reply.h>
#include <api/media_server_statistics_storage.h>

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
     *  Register the consumer object (usually widget).
     *
     * \param resource          Server resource whous history we want to receive.
     * \param target            Object that will be notified about new data.
     * \param slot              Slot that will be called when new data will be received.
     */
    void registerServerWidget(const QnMediaServerResourcePtr &resource, QObject *target, const char *slot);

    /**
     *  Unregister the consumer object (usually widget).
     *
     * \param resource          Server resource whous history we do not want to receive anymore.
     * \param target            Object that will not be notified about new data anymore.
     */
    void unregisterServerWidget(const QnMediaServerResourcePtr &resource, QObject *target);

    QnStatisticsHistory history(const QnMediaServerResourcePtr &resource) const;
    qint64 historyId(const QnMediaServerResourcePtr &resource) const;

    /** Data update period in milliseconds. It is taken from the server's response. */
    int updatePeriod(const QnMediaServerResourcePtr &resource) const;

    /** Number of data points that are stored simultaneously. */
    int pointsLimit() const;

private:
    QHash<QString, QnMediaServerStatisticsStorage *> m_statistics;
};

#endif // QN_STATISTICS_MANAGER
