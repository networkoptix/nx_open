#ifndef QN_STATISTICS_MANAGER
#define QN_STATISTICS_MANAGER

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QHash>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <api/video_server_statistics_data.h>

class QnStatisticsStorage: QObject{
    Q_OBJECT
public:
    QnStatisticsStorage(QObject *parent = NULL);
    qint64 getHistory(qint64 lastId, QnStatisticsHistory *history);
    bool isAlreadyUpdating(){return m_alreadyUpdating; }
    void update(QnVideoServerConnectionPtr apiConnection);
private slots:
    void at_statisticsReceived(const QnStatisticsDataList &);
private:
    bool m_alreadyUpdating;
    qint64 m_lastId;
    qint64 m_timeStamp;
    QnStatisticsHistory m_history;
};

class QnVideoServerStatisticsManager: QObject{
    Q_OBJECT
public:
    QnVideoServerStatisticsManager(QObject *parent = NULL): QObject(parent){}
    qint64 getHistory(QnVideoServerResourcePtr resource, qint64 lastId, QnStatisticsHistory *history);
private:
    QHash<QString, QnStatisticsStorage *> m_statistics;

};

//Q_DECLARE_METATYPE(QnVideoServerStatisticsManager)

#endif // QN_STATISTICS_MANAGER
