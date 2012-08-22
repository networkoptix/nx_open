#ifndef QN_STATISTICS_MANAGER
#define QN_STATISTICS_MANAGER

#include <QtCore/QString>
#include <QtCore/QHash>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <api/video_server_statistics_data.h>
#include <api/video_server_statistics_storage.h>

class QnVideoServerStatisticsManager: QObject{
    Q_OBJECT
public:
    QnVideoServerStatisticsManager(QObject *parent = NULL);
    qint64 getHistory(QnVideoServerResourcePtr resource, qint64 lastId, QnStatisticsHistory *history);
    int registerServerWidget(QnVideoServerResourcePtr resource, QObject *target, const char *slot);
    void unRegisterServerWidget(QnVideoServerResourcePtr resource, int widgetId, QObject *target);
    void notifyTimer(QnVideoServerResourcePtr resource, int widgetId);
private:
    QHash<QString, QnStatisticsStorage *> m_statistics;
};

#endif // QN_STATISTICS_MANAGER
