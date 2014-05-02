#ifndef QN_STATISTICS_REPLY_H
#define QN_STATISTICS_REPLY_H

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QLinkedList>

// TODO: 
// #MSAPI global enum without suffixes. Rename and move to some namespace (Qn probably).
// Note that it will be used in api.

enum QnStatisticsDeviceType {
    CPU,                /**< CPU load in percents. */
    RAM,                /**< RAM load in percents. */
    HDD,                /**< HDD load in percents. */
    NETWORK             /**< Network load in percent. */
};

struct QnStatisticsDataItem {
    QnStatisticsDataItem() {}
    QnStatisticsDataItem(const QString &description,
                         qreal value,
                         QnStatisticsDeviceType deviceType,
                         int deviceFlags = 0):
        description(description),
        value(value),
        deviceType(deviceType),
        deviceFlags(deviceFlags){}

    QString description;
    qreal value;
    QnStatisticsDeviceType deviceType;
    int deviceFlags;
};

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
    QnStatisticsDeviceType deviceType;
    QLinkedList<qreal> values;
};

typedef QHash<QString, QnStatisticsData> QnStatisticsHistory;

Q_DECLARE_METATYPE(QnStatisticsReply)
Q_DECLARE_METATYPE(QnStatisticsData)

#endif // QN_STATISTICS_REPLY_H
