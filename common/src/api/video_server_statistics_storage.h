#ifndef QN_STATISTICS_STORAGE
#define QN_STATISTICS_STORAGE

#include <QtCore/QString>
#include <QtCore/QHash>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <api/video_server_statistics_data.h>
#include <api/video_server_statistics_storage.h>

class QnStatisticsStorage: QObject{
    Q_OBJECT
public:
    QnStatisticsStorage(QObject *parent = NULL);
    qint64 getHistory(qint64 lastId, QnStatisticsHistory *history);
    int registerServerWidget(QObject *target, const char *slot);
    void unRegisterServerWidget(int widgetId, QObject *target);
    void notifyTimer(QnVideoServerConnectionPtr apiConnection, int widgetId);
    void update(QnVideoServerConnectionPtr apiConnection);
signals:
    void at_statistics_processed();
private slots:
    void at_statisticsReceived(const QnStatisticsDataList &data);
private:
    bool m_alreadyUpdating;
    qint64 m_lastId;
    qint64 m_timeStamp;
    int m_activeWidget;
    int m_lastWidget;

    QnStatisticsHistory m_history;
};

#endif // QN_STATISTICS_STORAGE
