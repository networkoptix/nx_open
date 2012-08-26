#ifndef QN_STATISTICS_MANAGER
#define QN_STATISTICS_MANAGER

#include <QtCore/QString>
#include <QtCore/QHash>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <api/video_server_statistics_data.h>
#include <api/video_server_statistics_storage.h>

/**
  * Class that receives, parses and stores statistics data from all servers.
  * Also handles request sending through an inner timer.
  */
class QnVideoServerStatisticsManager: public QObject {
    Q_OBJECT
public:
    /**
     * Constructor
     *
     * \param parent            Parent of the object
     */
    QnVideoServerStatisticsManager(QObject *parent = NULL);

    /**
     *  Register the consumer object (usually widget).
     *
     * \param resource          Server resource whous history we want to receive.
     * \param target            Object that will be notified about new data.
     * \param slot              Slot that will be called when new data will be received.
     */
    void registerServerWidget(QnVideoServerResourcePtr resource, QObject *target, const char *slot);

    /**
     *  Unregister the consumer object (usually widget).
     *
     * \param resource          Server resource whous history we do not want to receive anymore.
     * \param target            Object that will not be notified about new data anymore.
     */
    void unregisterServerWidget(QnVideoServerResourcePtr resource, QObject *target);

    /**
     *  Get history data for the selected server resource.
     *
     * \param resource          Server resource whous history we want to receive.
     * \param lastId            Id of the last response that is already processed by consumer.
     * \param history           Field that should be filled with results.
     * \returns                 Id of the last response in history.
     */
    qint64 getHistory(QnVideoServerResourcePtr resource, qint64 lastId, QnStatisticsHistory *history);

private slots:
    void at_timer_timeout();
private:
    QHash<QString, QnStatisticsStorage *> m_statistics;
};

#endif // QN_STATISTICS_MANAGER
