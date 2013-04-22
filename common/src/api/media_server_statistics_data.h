#ifndef QN_STATISTICS_DATA
#define QN_STATISTICS_DATA

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QLinkedList>

enum  QnStatisticsDeviceType {
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

struct QnStatisticsData{
    QString description;
    QnStatisticsDeviceType deviceType;
    QLinkedList<qreal> values;
};

typedef QHash<QString, QnStatisticsData> QnStatisticsHistory;

Q_DECLARE_METATYPE(QnStatisticsDataList)
Q_DECLARE_METATYPE(QnStatisticsData)

#endif // QN_STATISTICS_DATA
