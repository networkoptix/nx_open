#ifndef QN_MEDIA_SERVER_STATISTICS_STORAGE
#define QN_MEDIA_SERVER_STATISTICS_STORAGE

#include <QtCore/QString>
#include <QtCore/QHash>

#include <api/media_server_connection.h>
#include <core/resource/resource_fwd.h>
#include <api/model/statistics_reply.h>
#include <api/media_server_statistics_storage.h>

/**
  * Class that receives, parses and stores statistics data from one server.
  */
class QnMediaServerStatisticsStorage: QObject {
    Q_OBJECT
public:
    /**
     * Constructor
     *
     * \param server            Server resource to use.
     * \param parent            Parent object.
     */
    QnMediaServerStatisticsStorage(const QnMediaServerResourcePtr &server, int pointsLimit, QObject *parent);

    /**
     *  Register the consumer object.
     *
     * \param target            Object that will be notified about new data.
     * \param slot              Slot that will be called when new data will be received.
     */
    void registerConsumer(QObject *target, const char *slot);

    /**
     *  Unregister the consumer object.
     *
     * \param target            Object that will not be notified about new data anymore.
     */
    void unregisterConsumer(QObject *target);

    QnStatisticsHistory history() const;
    qint64 historyId() const;

    /** 
     * \returns                 Data update period. Is taken from the server's response. 
     */
    int updatePeriod() const;

    void setFlagsFilter(Qn::StatisticsDeviceType deviceType, int flags);

    qint64 uptimeMs() const;

signals:
    /**
     * This signal is emitted when new data is received.
     */
    void statisticsChanged();

private slots:
    /**
     *  Send update request to the server.
     */
    void update();

    /**
     * Private slot for the handling data received from the server.
     */
    void at_statisticsReceived(int status, const QnStatisticsReply &reply, int handle);

private:
    /** Number of update requests. Increased with every update period. */
    int m_updateRequests;

    /** Handle of the current update request. */
    int m_updateRequestHandle;

    qint64 m_lastId;
    qint64 m_timeStamp;
    uint m_listeners;
    int m_pointsLimit;
    int m_updatePeriod;
    qint64 m_uptimeMs;

    QnMediaServerResourcePtr m_server;
    QnStatisticsHistory m_history;
    QTimer *m_timer;
    QHash<Qn::StatisticsDeviceType, int> m_flagsFilter;
};

#endif // QN_MEDIA_SERVER_STATISTICS_STORAGE
