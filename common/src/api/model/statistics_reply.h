#ifndef QN_STATISTICS_REPLY_H
#define QN_STATISTICS_REPLY_H

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QLinkedList>


struct QnStatisticsDataItem {
    QnStatisticsDataItem() {}
    QnStatisticsDataItem(const QString &description,
                         qreal value,
                         Qn::StatisticsDeviceType deviceType,
                         int deviceFlags = 0):
        description(description),
        value(value),
        deviceType(deviceType),
        deviceFlags(deviceFlags){}

    QString description;
    qreal value;
    Qn::StatisticsDeviceType deviceType;
    int deviceFlags;
};

#define QnStatisticsDataItem_Fields (description)(value)(deviceType)(deviceFlags)

QN_FUSION_DECLARE_FUNCTIONS(QnStatisticsDataItem, (json)(metatype))


typedef QList<QnStatisticsDataItem> QnStatisticsDataList;

struct QnStatisticsReply {
    QnStatisticsReply(): updatePeriod(0), uptimeMs(0) {}
    
    QnStatisticsDataList statistics;
    qint64 updatePeriod;
    qint64 uptimeMs;
};


// TODO: #Elric move out

struct QnStatisticsData {
    QString description;
    Qn::StatisticsDeviceType deviceType;
    QLinkedList<qreal> values;
};

typedef QHash<QString, QnStatisticsData> QnStatisticsHistory;

Q_DECLARE_METATYPE(QnStatisticsData)

#define QnStatisticsReply_Fields (statistics)(updatePeriod)(uptimeMs)

QN_FUSION_DECLARE_FUNCTIONS(QnStatisticsReply, (json)(metatype))



#endif // QN_STATISTICS_REPLY_H
