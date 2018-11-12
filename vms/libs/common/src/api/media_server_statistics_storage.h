#pragma once

#include <QtCore/QString>
#include <QtCore/QHash>

#include <nx/utils/uuid.h>
#include <api/server_rest_connection_fwd.h>
#include <api/model/statistics_reply.h>
#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

class QTimer;

struct QnJsonRestResult;

/**
 * Class that receives, parses and stores statistics data from one server.
 */
class QnMediaServerStatisticsStorage: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

public:
    /** Construct a storage for the giver serverId. */
    QnMediaServerStatisticsStorage(const QnUuid& serverId, int pointsLimit, QObject* parent);

    virtual ~QnMediaServerStatisticsStorage() override;

    /**
     * Register a consumer object.
     * @param target An object to be notified about new data.
     * @param slot A slot to be called when new data is received.
     */
    void registerConsumer(QObject* target, const char* slot);

    /** Unregister a consumer object which was previously registered with registerConsumer(). */
    void unregisterConsumer(QObject* target);

    QnStatisticsHistory history() const;
    qint64 historyId() const;

    /** Returns the data update period. It is taken from the server response. */
    int updatePeriod() const;

    void setFlagsFilter(Qn::StatisticsDeviceType deviceType, int flags);

    qint64 uptimeMs() const;

signals:
    /** This signal is emitted when new data is received. */
    void statisticsChanged();

private slots:
    /** Send update request to the server. */
    void update();

    void handleStatisticsReply(bool success, rest::Handle handle, const QnJsonRestResult& result);

private:
    QnUuid m_serverId;

    /** Number of update requests. Increased with every update period. */
    int m_updateRequests = 0;

    /** Handle of the current update request. */
    rest::Handle m_updateRequestHandle = 0;

    qint64 m_lastId = -1;
    qint64 m_timeStamp = 0;
    uint m_listeners = 0;
    int m_pointsLimit = 0;
    int m_updatePeriod = 0;
    qint64 m_uptimeMs = 0;

    QnStatisticsHistory m_history;
    QTimer* m_timer = nullptr;
    QHash<Qn::StatisticsDeviceType, int> m_flagsFilter;
};
