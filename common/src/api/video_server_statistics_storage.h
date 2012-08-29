#ifndef QN_STATISTICS_STORAGE
#define QN_STATISTICS_STORAGE

#include <QtCore/QString>
#include <QtCore/QHash>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <api/video_server_statistics_data.h>
#include <api/video_server_statistics_storage.h>

/**
  * Class that receives, parses and stores statistics data from one server.
  */
class QnStatisticsStorage: QObject{
    Q_OBJECT
public:
    /**
     * Constructor
     *
     * \param apiConnection     Api connection of the server that will provide the statistics.
     * \param parent            Parent of the object
     */
    QnStatisticsStorage(const QnVideoServerConnectionPtr &apiConnection, QObject *parent);

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

    // TODO: #GDM #1.4 I think we can have a simpler API here, like 
    // QnStatisticsHistory history() const;
    // This way we'll avoid the need to store request ID at the caller's side.
    /**
     *  Get history data for the selected server resource.
     *
     * \param lastId            Id of the last response that is already processed by consumer.
     * \param history           Field that should be filled with results.
     * \returns                 Id of the last response in history.
     */
    qint64 getHistory(qint64 lastId, QnStatisticsHistory *history);

    /**
     *  Send update request to the server.
     */
    void update();

signals:
    // TODO: #GDM rename to statisticsUpdated? Or statisticsChanged?
    // at_somewhere_somethingHappened() is a name template for signal handlers,
    // somethingHappened() --- for signals.

    /**
     * Signal emitted when new data is received.
     */
    void at_statistics_processed(); //naming conventions?
private slots:

    /**
     * Private slot for the handling data received from the server.
     */
    void at_statisticsReceived(const QnStatisticsDataList &data);
private:
    bool m_alreadyUpdating;
    qint64 m_lastId;
    qint64 m_timeStamp;
    uint m_listeners;

    QnStatisticsHistory m_history;
    QnVideoServerConnectionPtr m_apiConnection;
};

#endif // QN_STATISTICS_STORAGE
