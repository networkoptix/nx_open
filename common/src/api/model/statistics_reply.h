#ifndef QN_STATISTICS_REPLY_H
#define QN_STATISTICS_REPLY_H

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QLinkedList>

enum QnStatisticsDeviceType {
    CPU,                /**< CPU load in percents */
    RAM,                /**< RAM load in percents */
    HDD,                /**< HDD load in percents */
    NETWORK_IN,         /**< Network load in bits per sec - inbound traffic */
    NETWORK_OUT         /**< Network load in bits per sec - outbound traffic */
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
