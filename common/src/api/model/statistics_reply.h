#ifndef QN_STATISTICS_REPLY_H
#define QN_STATISTICS_REPLY_H

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QLinkedList>

enum QnStatisticsDeviceType {
    CPU,
    RAM,
    HDD,
    NETWORK_IN,
    NETWORK_OUT
};

struct QnStatisticsDataItem {
    QnStatisticsDataItem() {}
    QnStatisticsDataItem(const QString &description, qreal value, QnStatisticsDeviceType deviceType): description(description), value(value), deviceType(deviceType) {}

    QString description;
    qreal value;
    QnStatisticsDeviceType deviceType;
};

typedef QList<QnStatisticsDataItem> QnStatisticsDataList;

struct QnStatisticsReply {
    QnStatisticsDataList statistics;
    qint64 updatePeriod;
};


// TODO: #Elric move out

struct QnStatisticsData {
    QString description;
    QnStatisticsDeviceType deviceType;
    QLinkedList<qreal> values;
};

typedef QHash<QString, QnStatisticsData> QnStatisticsHistory;

Q_DECLARE_METATYPE(QnStatisticsReply)
Q_DECLARE_METATYPE(QnStatisticsData)

#endif // QN_STATISTICS_REPLY_H
