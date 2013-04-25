#ifndef QN_MEDIA_SERVER_STATISTICS_STORAGE
#define QN_MEDIA_SERVER_STATISTICS_STORAGE

#include <QtCore/QString>
#include <QtCore/QHash>

#include <api/media_server_connection.h>
#include <core/resource/resource_fwd.h>
#include <api/media_server_statistics_data.h>
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
     * \param apiConnection     Api connection of the server that will provide the statistics.
     * \param parent            Parent of the object
     */
    QnMediaServerStatisticsStorage(const QnMediaServerConnectionPtr &apiConnection, int pointsLimit, QObject *parent);

    // TODO: #Elric #1.4 Signal exposure + connectNotify/disconnectNotify is a more Qt-ish way to do this.
    /**
     *  Register the consumer object (usually widget).
     *
     * \param target            Object that will be notified about new data.
     * \param slot              Slot that will be called when new data will be received.
     */
    void registerServerWidget(QObject *target, const char *slot);

    /**
     *  Unregister the consumer object (usually widget).
     *
     * \param target            Object that will not be notified about new data anymore.
     */
    void unregisterServerWidget(QObject *target);

    QnStatisticsHistory history() const;
    qint64 historyId() const;

    /** Data update period. Is taken from the server's response. */
    int updatePeriod() const;
signals:
    /**
     * Signal emitted when new data is received.
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
    void at_statisticsReceived(int status, const QnStatisticsDataList &data, int updatePeriod, int handle);

private:
    bool m_alreadyUpdating;
    qint64 m_lastId;
    qint64 m_timeStamp;
    uint m_listeners;
    int m_pointsLimit;
    int m_updatePeriod;

    QnStatisticsHistory m_history;
    QnMediaServerConnectionPtr m_apiConnection;
    QTimer* m_timer;
};

#endif // QN_MEDIA_SERVER_STATISTICS_STORAGE
